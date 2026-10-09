#include "stdafx.h"
#include "questlua.h"
#include "questmanager.h"
#include "desc_client.h"
#include "char.h"
#include "item_manager.h"
#include "item.h"
#include "cmd.h"
#include "packet.h"

#ifdef ENABLE_DICE_SYSTEM
#include "party.h"
#endif

#undef sys_err
#ifndef __WIN32__
#define sys_err(fmt, args...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define sys_err(fmt, ...) quest::CQuestManager::instance().QuestError(__FUNCTION__, __LINE__, fmt, __VA_ARGS__)
#endif

extern ACMD(do_in_game_mall);

// MT2009_PLUS_CATCH_KING_V1 (declare): Catch the King's quest side (playerbot_catchking.h).
int CatchKingRanking(bool total, std::vector<std::string>& names, std::vector<int>& empires, std::vector<DWORD>& scores);
DWORD CatchKingMyScore(LPCHARACTER ch, bool total);
int CatchKingClaimReward(LPCHARACTER ch);
bool CatchKingUseItem(LPCHARACTER ch, DWORD vnum);
void CatchKingDeliver(LPCHARACTER ch);

namespace quest
{
	ALUA(game_set_event_flag)
	{
		CQuestManager & q = CQuestManager::instance();

		if (lua_isstring(L,1) && lua_isnumber(L, 2))
			q.RequestSetEventFlag(lua_tostring(L,1), (int)lua_tonumber(L,2));

		return 0;
	}

	ALUA(game_get_event_flag)
	{
		CQuestManager& q = CQuestManager::instance();

		if (lua_isstring(L,1))
			lua_pushnumber(L, q.GetEventFlag(lua_tostring(L,1)));
		else
			lua_pushnumber(L, 0);

		return 1;
	}

	ALUA(game_request_make_guild)
	{
		CQuestManager& q = CQuestManager::instance();
		LPDESC d = q.GetCurrentCharacterPtr()->GetDesc();
		if (d)
		{
			BYTE header = HEADER_GC_REQUEST_MAKE_GUILD;
			d->Packet(&header, 1);
		}
		return 0;
	}

	ALUA(game_get_safebox_level)
	{
		CQuestManager& q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		if (!ch)
			return 0;

		lua_pushnumber(L, ch->GetSafeboxSize());
		return 1;
	}

	ALUA(game_set_safebox_level)
	{
		CQuestManager& q = CQuestManager::instance();

		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		if (!ch)
			return 0;

		int size = (int)lua_tonumber(L, -1);
		if (ch->GetSafeboxSize() >= size)
			return 0;

		LPDESC d = ch->GetDesc();
		if (!d)
			return 0;
		
		TSafeboxChangeSizePacket p;
		p.dwID = q.GetCurrentCharacterPtr()->GetDesc()->GetAccountTable().id;
		p.bSize = size;
		db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_SIZE,  q.GetCurrentCharacterPtr()->GetDesc()->GetHandle(), &p, sizeof(p));
		return 0;
	}

	ALUA(game_open_safebox)
	{
		CQuestManager& q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		ch->SetSafeboxOpenPosition();
		ch->ChatPacket(CHAT_TYPE_COMMAND, "ShowMeSafeboxPassword");
		return 0;
	}

	ALUA(game_open_mall)
	{
		CQuestManager& q = CQuestManager::instance();
		LPCHARACTER ch = q.GetCurrentCharacterPtr();
		ch->SetSafeboxOpenPosition();
		ch->ChatPacket(CHAT_TYPE_COMMAND, "ShowMeMallPassword");
		return 0;
	}

	ALUA(game_drop_item)
	{
		// Syntax: game.drop_item(50050, 1)

		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		DWORD item_vnum = (DWORD) lua_tonumber(L, 1);
		int count = (int) lua_tonumber(L, 2);
		long x = ch->GetX();
		long y = ch->GetY();

		LPITEM item = ITEM_MANAGER::instance().CreateItem(item_vnum, count);

		if (!item)
		{
			sys_err("cannot create item vnum %d count %d", item_vnum, count);
			return 0;
		}

		PIXEL_POSITION pos;
		pos.x = x + number(-200, 200);
		pos.y = y + number(-200, 200);

		item->AddToGround(ch->GetMapIndex(), pos);
		item->StartDestroyEvent();

		return 0;
	}

	ALUA(game_drop_item_with_ownership)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		LPITEM item = NULL;
		switch (lua_gettop(L))
		{
		case 1:
			item = ITEM_MANAGER::instance().CreateItem((DWORD) lua_tonumber(L, 1));
			break;
		case 2:
		case 3:
			item = ITEM_MANAGER::instance().CreateItem((DWORD) lua_tonumber(L, 1), (int) lua_tonumber(L, 2));
			break;
		default:
			return 0;
		}

		if ( item == NULL )
		{
			return 0;
		}

		if (lua_isnumber(L, 3))
		{
			int sec = (int) lua_tonumber(L, 3);
			if (sec <= 0)
			{
				item->SetOwnership( ch );
			}
			else
			{
				item->SetOwnership( ch, sec );
			}
		}
		else
			item->SetOwnership( ch );

		PIXEL_POSITION pos;
		pos.x = ch->GetX() + number(-200, 200);
		pos.y = ch->GetY() + number(-200, 200);

		item->AddToGround(ch->GetMapIndex(), pos);
		item->StartDestroyEvent();

		return 0;
	}

#ifdef ENABLE_DICE_SYSTEM
	ALUA(game_drop_item_with_ownership_and_dice)
	{
		LPITEM item = NULL;
		switch (lua_gettop(L))
		{
		case 1:
			item = ITEM_MANAGER::instance().CreateItem((DWORD) lua_tonumber(L, 1));
			break;
		case 2:
		case 3:
			item = ITEM_MANAGER::instance().CreateItem((DWORD) lua_tonumber(L, 1), (int) lua_tonumber(L, 2));
			break;
		default:
			return 0;
		}

		if ( item == NULL )
		{
			return 0;
		}

		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		if (ch->GetParty())
		{
			FPartyDropDiceRoll f(item, ch);
			f.Process(NULL);
		}

		if (lua_isnumber(L, 3))
		{
			int sec = (int) lua_tonumber(L, 3);
			if (sec <= 0)
			{
				item->SetOwnership( ch );
			}
			else
			{
				item->SetOwnership( ch, sec );
			}
		}
		else
			item->SetOwnership( ch );

		PIXEL_POSITION pos;
		pos.x = ch->GetX() + number(-200, 200);
		pos.y = ch->GetY() + number(-200, 200);

		item->AddToGround(ch->GetMapIndex(), pos);
		item->StartDestroyEvent();

		return 0;
	}
#endif

	ALUA(game_web_mall)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();

		if ( ch != NULL )
		{
			do_in_game_mall(ch, const_cast<char*>(""), 0, 0);
		}
		return 0;
	}

	// MT2009_PLUS_RUMI_V1 (lua): Owsap's Rumi table (minigame_rumi.quest), playerbot_rumi.h.
	void RumiLuaScoreTable(bool total, std::vector<std::pair<std::string, DWORD> >& out);
	DWORD RumiLuaMyScore(LPCHARACTER ch, bool total);
	int RumiLuaClaim(LPCHARACTER ch, DWORD& vnum, int& count);
	int RumiLuaPending(LPCHARACTER ch);
	DWORD RumiLuaPrize();

	// game.get_minigame_rumi_score(total): { {name, score}, ... } - the season's top ten
	ALUA(game_get_minigame_rumi_score)
	{
		std::vector<std::pair<std::string, DWORD> > top;
		RumiLuaScoreTable(lua_toboolean(L, 1) != 0, top);
		lua_newtable(L);
		for (size_t i = 0; i < top.size(); ++i)
		{
			lua_newtable(L);
			lua_pushstring(L, top[i].first.c_str());
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, top[i].second);
			lua_rawseti(L, -2, 2);
			lua_rawseti(L, -2, (int)i + 1);
		}
		return 1;
	}

	// game.get_minigame_rumi_my_score(total)
	ALUA(game_get_minigame_rumi_my_score)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		lua_pushnumber(L, ch ? RumiLuaMyScore(ch, lua_toboolean(L, 1) != 0) : 0);
		return 1;
	}

	// game.minigame_rumi_claim(): code (0 given, 1 no window, 2 taken, 3 not ranked, 4 no room, 5 no row), vnum, count
	ALUA(game_minigame_rumi_claim)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		DWORD vnum = 0;
		int count = 0;
		const int code = ch ? RumiLuaClaim(ch, vnum, count) : 1;
		lua_pushnumber(L, code);
		lua_pushnumber(L, vnum);
		lua_pushnumber(L, count);
		return 3;
	}

	// game.minigame_rumi_pending(): the chests a game left at logout owes, given now
	ALUA(game_minigame_rumi_pending)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		lua_pushnumber(L, ch ? RumiLuaPending(ch) : 0);
		return 1;
	}

	// game.minigame_rumi_prize(): the top ten's chest
	ALUA(game_minigame_rumi_prize)
	{
		lua_pushnumber(L, RumiLuaPrize());
		return 1;
	}

	// MT2009_PLUS_CATCH_KING_V1 (lua): Catch the King (minigame_catchking.quest).
	// game.get_catchking_score(total) -> { {name, empire, score}, ... } the season's top ten
	// (Owsap's name and shape), game.get_catchking_myscore(total), game.catchking_claim_reward()
	// -> the Golden Loots given / 0 not in the top ten / -1 no window / -2 taken / -3 the event
	// runs / -4 a full bag, game.catchking_use_item(vnum) -> true when the card or deck counted,
	// game.catchking_deliver() a left game's Loot at login.
	ALUA(game_get_catchking_score)
	{
		std::vector<std::string> names;
		std::vector<int> empires;
		std::vector<DWORD> scores;
		const int n = ::CatchKingRanking(lua_toboolean(L, 1) != 0, names, empires, scores);
		lua_newtable(L);
		for (int i = 0; i < n; ++i)
		{
			lua_newtable(L);
			lua_pushstring(L, names[i].c_str());
			lua_rawseti(L, -2, 1);
			lua_pushnumber(L, empires[i]);
			lua_rawseti(L, -2, 2);
			lua_pushnumber(L, scores[i]);
			lua_rawseti(L, -2, 3);
			lua_rawseti(L, -2, i + 1);
		}
		return 1;
	}

	ALUA(game_get_catchking_myscore)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		lua_pushnumber(L, ch ? ::CatchKingMyScore(ch, lua_toboolean(L, 1) != 0) : 0);
		return 1;
	}

	ALUA(game_catchking_claim_reward)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		lua_pushnumber(L, ch ? ::CatchKingClaimReward(ch) : -1);
		return 1;
	}

	ALUA(game_catchking_use_item)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		lua_pushboolean(L, ch && lua_isnumber(L, 1) && ::CatchKingUseItem(ch, (DWORD)lua_tonumber(L, 1)));
		return 1;
	}

	ALUA(game_catchking_deliver)
	{
		LPCHARACTER ch = CQuestManager::instance().GetCurrentCharacterPtr();
		if (ch)
			::CatchKingDeliver(ch);
		return 0;
	}

	void RegisterGameFunctionTable()
	{
		luaL_reg game_functions[] =
		{
			{ "get_safebox_level",			game_get_safebox_level			},
			{ "request_make_guild",			game_request_make_guild			},
			{ "set_safebox_level",			game_set_safebox_level			},
			{ "open_safebox",				game_open_safebox				},
			{ "open_mall",					game_open_mall					},
			{ "get_event_flag",				game_get_event_flag				},
			{ "set_event_flag",				game_set_event_flag				},
			{ "drop_item",					game_drop_item					},
			{ "drop_item_with_ownership",	game_drop_item_with_ownership	},
#ifdef ENABLE_DICE_SYSTEM
			{ "drop_item_with_ownership_and_dice",	game_drop_item_with_ownership_and_dice	},
#endif
			{ "open_web_mall",				game_web_mall					},
			// MT2009_PLUS_CATCH_KING_V1 (table)
			{ "get_catchking_score",		game_get_catchking_score		},
			{ "get_catchking_myscore",		game_get_catchking_myscore		},
			{ "catchking_claim_reward",		game_catchking_claim_reward		},
			{ "catchking_use_item",			game_catchking_use_item			},
			{ "catchking_deliver",			game_catchking_deliver			},
			// MT2009_PLUS_RUMI_V1 (table)
			{ "get_minigame_rumi_score",	game_get_minigame_rumi_score	},
			{ "get_minigame_rumi_my_score",	game_get_minigame_rumi_my_score	},
			{ "minigame_rumi_claim",		game_minigame_rumi_claim		},
			{ "minigame_rumi_pending",		game_minigame_rumi_pending		},
			{ "minigame_rumi_prize",		game_minigame_rumi_prize		},

			{ NULL,					NULL				}
		};

		CQuestManager::instance().AddLuaFunctionTable("game", game_functions);
	}
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
