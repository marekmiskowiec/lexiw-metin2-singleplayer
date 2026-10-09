#include "stdafx.h"
#include <set>
#include <vector>
#include <map> // MT2009_PLUS_DROP_WIKI_V1
#include <algorithm>
#if defined(__FreeBSD__) || defined(__linux__)
#include <md5.h>
#else
#include "../../libthecore/include/xmd5.h"
#endif

#include "utils.h"
#include "config.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "char.h"
#include "char_manager.h"
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
#include "MountSystem.h"
#endif
#include "motion.h"
#include "packet.h"
#include "affect.h"
#include "pvp.h"
#include "start_position.h"
#include "party.h"
#include "guild_manager.h"
#include "p2p.h"
#include "dungeon.h"
#include "messenger_manager.h"
#include "war_map.h"
#include "playerbot_manager.h"
#include "questmanager.h"
#include "item_manager.h"
#include "monarch.h"
#include "mob_manager.h"
#include "item.h"
#include "arena.h"
#include "buffer_manager.h"
#include "unique_item.h"
#include "threeway_war.h"
#include "log.h"
#include "sectree_manager.h"
#include "battle.h"
#include "playerbot_arrange.h"
#include "../../common/VnumHelper.h"
#include "../../common/PulseManager.h"

ACMD(do_user_horse_ride)
{
	if (ch->IsObserverMode())
		return;

	if (ch->IsDead() || ch->IsStun() || ch->IsBusyAction())
		return;

	if (ch->IsHorseRiding() == false)
	{
		if (ch->GetMountVnum())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You're already riding. Get off first."));
			return;
		}

		if (ch->GetHorse() == NULL)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Please call your Horse first."));
			return;
		}

		if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::HorseUse, std::chrono::seconds(1)))
		{
			return;
		}

		ch->StartRiding();
	}
	else
	{
		ch->StopRiding();
	}
}

ACMD(do_user_horse_back)
{
	if (ch->IsBusy())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_CONTINUE_WHEN_BUSY"));
		return;
	}

	if (ch->GetHorse() != NULL)
	{
		ch->HorseSummon(false);
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have sent your horse away."));
	}
	else if (ch->IsHorseRiding() == true)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have to get off your Horse."));
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Please call your Horse first."));
	}
}

ACMD(do_user_horse_feed)
{
	if (ch->GetMyShop())
		return;

	if (ch->GetHorse() == NULL)
	{
		if (ch->IsHorseRiding() == false)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Please call your Horse first."));
		else
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot feed your Horse whilst sitting on it."));
		return;
	}

	DWORD dwFood = ch->GetHorseGrade() + 50054 - 1;

	auto foodTable = ITEM_MANAGER::instance().GetTable(dwFood);
	auto foodName = (foodTable) ? foodTable->szLocaleName : "NONAME"; // @fixme307

	if (ch->CountSpecifyItem(dwFood) > 0)
	{
		ch->RemoveSpecifyItem(dwFood, 1);
		ch->FeedHorse();
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have fed the Horse with %s%s."), foodName, "");
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need %s."), foodName);
	}
}

#define MAX_REASON_LEN		128

EVENTINFO(TimedEventInfo)
{
	DynamicCharacterPtr ch;
	int		subcmd;
	int         	left_second;
	char		szReason[MAX_REASON_LEN];

	TimedEventInfo()
	: ch()
	, subcmd( 0 )
	, left_second( 0 )
	{
		::memset( szReason, 0, MAX_REASON_LEN );
	}
};

struct SendDisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetCharacter())
		{
			if (d->GetCharacter()->GetGMLevel() == GM_PLAYER)
				d->GetCharacter()->ChatPacket(CHAT_TYPE_COMMAND, "quit Shutdown(SendDisconnectFunc)");
		}
	}
};

struct DisconnectFunc
{
	void operator () (LPDESC d)
	{
		if (d->GetType() == DESC_TYPE_CONNECTOR)
			return;

		if (d->IsPhase(PHASE_P2P))
			return;

		if (d->GetCharacter())
			d->GetCharacter()->Disconnect("Shutdown(DisconnectFunc)");

		d->SetPhase(PHASE_CLOSE);
	}
};

EVENTINFO(shutdown_event_data)
{
	int seconds;

	shutdown_event_data()
	: seconds( 0 )
	{
	}
};

EVENTFUNC(shutdown_event)
{
	shutdown_event_data* info = dynamic_cast<shutdown_event_data*>( event->info );

	if ( info == NULL )
	{
		sys_err( "shutdown_event> <Factor> Null pointer" );
		return 0;
	}

	int * pSec = & (info->seconds);

	if (*pSec < 0)
	{
		sys_log(0, "shutdown_event sec %d", *pSec);

		if (--*pSec == -10)
		{
			const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
			std::for_each(c_set_desc.begin(), c_set_desc.end(), DisconnectFunc());
			return passes_per_sec;
		}
		else if (*pSec < -10)
			return 0;

		return passes_per_sec;
	}
	else if (*pSec == 0)
	{
		const DESC_MANAGER::DESC_SET & c_set_desc = DESC_MANAGER::instance().GetClientSet();
		std::for_each(c_set_desc.begin(), c_set_desc.end(), SendDisconnectFunc());
		g_bNoMoreClient = true;
		--*pSec;
		return passes_per_sec;
	}
	else
	{
		char buf[64];
		snprintf(buf, sizeof(buf), LC_TEXT("%d seconds until Exit."), *pSec);
		SendNotice(buf);

		--*pSec;
		return passes_per_sec;
	}
}

void Shutdown(int iSec)
{
	if (g_bNoMoreClient)
	{
		thecore_shutdown();
		return;
	}

	CWarMapManager::instance().OnShutdown();

	char buf[64];
	snprintf(buf, sizeof(buf), LC_TEXT("The game will be closed in %d seconds."), iSec);

	SendNotice(buf);

	shutdown_event_data* info = AllocEventInfo<shutdown_event_data>();
	info->seconds = iSec;

	event_create(shutdown_event, info, 1);
}

ACMD(do_shutdown)
{
	sys_err("Accept shutdown command from %s.", (ch) ? ch->GetName() : "NONAME");

	TPacketGGShutdown p;
	p.bHeader = HEADER_GG_SHUTDOWN;
	P2P_MANAGER::instance().Send(&p, sizeof(TPacketGGShutdown));

	Shutdown(10);
}

EVENTFUNC(timed_event)
{
	TimedEventInfo * info = dynamic_cast<TimedEventInfo *>( event->info );

	if ( info == NULL )
	{
		sys_err( "timed_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER	ch = info->ch;
	if (ch == NULL) { // <Factor>
		return 0;
	}
	LPDESC d = ch->GetDesc();

	if (info->left_second <= 0)
	{
		ch->m_pkTimedEvent = NULL;

		if (ch->IsBusy(BUSY_WARP))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your logout has been cancelled."));
			return 0;
		}

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
			case SCMD_QUIT:
			case SCMD_PHASE_SELECT:
				{
				TPacketNeedLoginLogInfo acc_info{};
					acc_info.dwPlayerID = ch->GetDesc()->GetAccountTable().id;

					db_clientdesc->DBPacket( HEADER_GD_VALID_LOGOUT, 0, &acc_info, sizeof(acc_info) );

					LogManager::instance().DetailLoginLog( false, ch );
				}
				break;
		}

		switch (info->subcmd)
		{
			case SCMD_LOGOUT:
				if (d)
					d->SetPhase(PHASE_CLOSE);
				break;

			case SCMD_QUIT:
				ch->ChatPacket(CHAT_TYPE_COMMAND, "quit");
				if (d) // @fixme197
					d->DelayedDisconnect(1);
				break;

			case SCMD_PHASE_SELECT:
				ch->Disconnect("timed_event - SCMD_PHASE_SELECT");
				if (d)
					d->SetPhase(PHASE_SELECT);
				break;
		}

		return 0;
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%d seconds until Exit."), info->left_second);
		--info->left_second;
	}

	return PASSES_PER_SEC(1);
}

ACMD(do_cmd)
{
	if (ch->m_pkTimedEvent)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your logout has been cancelled."));
		event_cancel(&ch->m_pkTimedEvent);
		return;
	}

	if (ch->IsBusy(BUSY_WARP))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_CONTINUE_WHEN_BUSY"));
		return;
	}

	if (ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_WHILE_DEAD"));
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Back to login window. Please wait."));
			break;

		case SCMD_QUIT:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have been disconnected from the server. Please wait."));
			break;

		case SCMD_PHASE_SELECT:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are changing character. Please wait."));
			break;
	}

	int nExitLimitTime = 10;

	if (!ch->CanWarp())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("After opening the Storeroom, you cannot go anywhere else for %d seconds."), nExitLimitTime);
		return;
	}

	if (ch->IsHack(false, true, nExitLimitTime) &&
		false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()) &&
	   	(!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG))
	{
		return;
	}

	switch (subcmd)
	{
		case SCMD_LOGOUT:
		case SCMD_QUIT:
		case SCMD_PHASE_SELECT:
			{
				TimedEventInfo* info = AllocEventInfo<TimedEventInfo>();

				{
					if (ch->IsPosition(POS_FIGHTING))
						info->left_second = 10;
					else
						info->left_second = 3;
				}

				info->ch		= ch;
				info->subcmd		= subcmd;
				strlcpy(info->szReason, argument, sizeof(info->szReason));

				ch->m_pkTimedEvent	= event_create(timed_event, info, 1);
			}
			break;
	}
}

ACMD(do_mount)
{
}

ACMD(do_fishing)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	ch->SetRotation(atof(arg1));
	ch->fishing();
}

ACMD(do_console)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ConsoleEnable");
}

ACMD(do_restart)
{
	if (false == ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");
		ch->StartRecoveryEvent();
		return;
	}

	if (NULL == ch->m_pkDeadEvent)
		return;

	int iTimeToDead = (event_time(ch->m_pkDeadEvent) / passes_per_sec);

	if (subcmd != SCMD_RESTART_TOWN)
	{
		if (!test_server)
		{
			if (ch->IsHack())
			{
				if (false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("A new start is not possible at the moment. Please wait %d seconds."), iTimeToDead - (180 - g_nPortalLimitTime));
					return;
				}
			}
#define eFRS_HERESEC	170
			if (iTimeToDead > eFRS_HERESEC)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("A new start is not possible at the moment. Please wait %d seconds."), iTimeToDead - eFRS_HERESEC);
				return;
			}
		}
	}

	//PREVENT_HACK

	if (subcmd == SCMD_RESTART_TOWN)
	{
		if (ch->IsHack())
		{
			if ((!ch->GetWarMap() || ch->GetWarMap()->GetType() == GUILD_WAR_TYPE_FLAG) ||
			   	false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("A new start is not possible at the moment. Please wait %d seconds."), iTimeToDead - (180 - g_nPortalLimitTime));
				return;
			}
		}

#define eFRS_TOWNSEC	173
		if (iTimeToDead > eFRS_TOWNSEC)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot restart in the city yet. Wait another %d seconds."), iTimeToDead - eFRS_TOWNSEC);
			return;
		}
	}
	//END_PREVENT_HACK

	ch->ChatPacket(CHAT_TYPE_COMMAND, "CloseRestartWindow");

	ch->GetDesc()->SetPhase(PHASE_GAME);
	ch->SetPosition(POS_STANDING);
	ch->StartRecoveryEvent();

	//FORKED_LOAD

	if (1 == quest::CQuestManager::instance().GetEventFlag("threeway_war"))
	{
		if (subcmd == SCMD_RESTART_TOWN || subcmd == SCMD_RESTART_HERE)
		{
			if (true == CThreeWayWar::instance().IsThreeWayWarMapIndex(ch->GetMapIndex()) &&
					false == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));

				ch->ReviveInvisible(5);
				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());

				return;
			}

			if (true == CThreeWayWar::instance().IsSungZiMapIndex(ch->GetMapIndex()))
			{
				if (CThreeWayWar::instance().GetReviveTokenForPlayer(ch->GetPlayerID()) <= 0)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The waiting time has expired. You will be revived in the city."));
					ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
				}
				else
				{
					ch->Show(ch->GetMapIndex(), GetSungziStartX(ch->GetEmpire()), GetSungziStartY(ch->GetEmpire()));
				}

				ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
				ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
				ch->ReviveInvisible(5);

				return;
			}
		}
	}
	//END_FORKED_LOAD

	if (ch->GetDungeon())
		ch->GetDungeon()->UseRevive(ch);

	if (ch->GetWarMap() && !ch->IsObserverMode())
	{
		CWarMap * pMap = ch->GetWarMap();
		DWORD dwGuildOpponent = pMap ? pMap->GetGuildOpponent(ch) : 0;

		if (dwGuildOpponent)
		{
			switch (subcmd)
			{
				case SCMD_RESTART_TOWN:
					sys_log(0, "do_restart: restart town");
					PIXEL_POSITION pos;

					if (CWarMapManager::instance().GetStartPosition(ch->GetMapIndex(), ch->GetGuild()->GetID() < dwGuildOpponent ? 0 : 1, pos))
						ch->Show(ch->GetMapIndex(), pos.x, pos.y);
					else
						ch->ExitToSavedLocation();

					ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible(5);
					break;

				case SCMD_RESTART_HERE:
					sys_log(0, "do_restart: restart here");
					ch->RestartAtSamePos();
					//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
					ch->PointChange(POINT_HP, ch->GetMaxHP() - ch->GetHP());
					ch->PointChange(POINT_SP, ch->GetMaxSP() - ch->GetSP());
					ch->ReviveInvisible(5);
					break;
			}

			return;
		}
	}
	switch (subcmd)
	{
		case SCMD_RESTART_TOWN:
			sys_log(0, "do_restart: restart town");
			PIXEL_POSITION pos;

			if (SECTREE_MANAGER::instance().GetRecallPositionByEmpire(ch->GetMapIndex(), ch->GetEmpire(), pos))
				ch->WarpSet(pos.x, pos.y);
			else
				ch->WarpSet(EMPIRE_START_X(ch->GetEmpire()), EMPIRE_START_Y(ch->GetEmpire()));
			ch->PointChange(POINT_HP, 50 - ch->GetHP());
			ch->DeathPenalty(1);
			break;

		case SCMD_RESTART_HERE:
			sys_log(0, "do_restart: restart here");
			ch->RestartAtSamePos();
			//ch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY());
			ch->PointChange(POINT_HP, 50 - ch->GetHP());
			ch->DeathPenalty(0);
			ch->ReviveInvisible(5);
			break;
	}
}

#define MAX_STAT g_iStatusPointSetMaxValue

ACMD(do_stat_reset)
{
	ch->PointChange(POINT_STAT_RESET_COUNT, 12 - ch->GetPoint(POINT_STAT_RESET_COUNT));
}

ACMD(do_stat_minus)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change your status while you are transformed."));
		return;
	}

	if (ch->GetPoint(POINT_STAT_RESET_COUNT) <= 0)
		return;

	if (!strcmp(arg1, "st"))
	{
		if (ch->GetRealPoint(POINT_ST) <= JobInitialPoints[ch->GetJob()].st)
			return;

		ch->SetRealPoint(POINT_ST, ch->GetRealPoint(POINT_ST) - 1);
		ch->SetPoint(POINT_ST, ch->GetPoint(POINT_ST) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_ST, 0);
	}
	else if (!strcmp(arg1, "dx"))
	{
		if (ch->GetRealPoint(POINT_DX) <= JobInitialPoints[ch->GetJob()].dx)
			return;

		ch->SetRealPoint(POINT_DX, ch->GetRealPoint(POINT_DX) - 1);
		ch->SetPoint(POINT_DX, ch->GetPoint(POINT_DX) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_DX, 0);
	}
	else if (!strcmp(arg1, "ht"))
	{
		if (ch->GetRealPoint(POINT_HT) <= JobInitialPoints[ch->GetJob()].ht)
			return;

		ch->SetRealPoint(POINT_HT, ch->GetRealPoint(POINT_HT) - 1);
		ch->SetPoint(POINT_HT, ch->GetPoint(POINT_HT) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_HT, 0);
		ch->PointChange(POINT_MAX_HP, 0);
	}
	else if (!strcmp(arg1, "iq"))
	{
		if (ch->GetRealPoint(POINT_IQ) <= JobInitialPoints[ch->GetJob()].iq)
			return;

		ch->SetRealPoint(POINT_IQ, ch->GetRealPoint(POINT_IQ) - 1);
		ch->SetPoint(POINT_IQ, ch->GetPoint(POINT_IQ) - 1);
		ch->ComputePoints();
		ch->PointChange(POINT_IQ, 0);
		ch->PointChange(POINT_MAX_SP, 0);
	}
	else
		return;

	ch->PointChange(POINT_STAT, +1);
	ch->PointChange(POINT_STAT_RESET_COUNT, -1);
	ch->ComputePoints();
}

ACMD(do_stat)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (ch->IsPolymorphed())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change your status while you are transformed."));
		return;
	}

	if (ch->GetPoint(POINT_STAT) <= 0)
		return;

	BYTE idx = 0;

	if (!strcmp(arg1, "st"))
		idx = POINT_ST;
	else if (!strcmp(arg1, "dx"))
		idx = POINT_DX;
	else if (!strcmp(arg1, "ht"))
		idx = POINT_HT;
	else if (!strcmp(arg1, "iq"))
		idx = POINT_IQ;
	else
		return;

	// ch->ChatPacket(CHAT_TYPE_INFO, "%s GRP(%d) idx(%u), MAX_STAT(%d), expr(%d)", __FUNCTION__, ch->GetRealPoint(idx), idx, MAX_STAT, ch->GetRealPoint(idx) >= MAX_STAT);
	if (ch->GetRealPoint(idx) >= MAX_STAT)
		return;

	ch->SetRealPoint(idx, ch->GetRealPoint(idx) + 1);
	ch->SetPoint(idx, ch->GetPoint(idx) + 1);
	ch->ComputePoints();
	ch->PointChange(idx, 0);

	if (idx == POINT_IQ)
	{
		ch->PointChange(POINT_MAX_HP, 0);
	}
	else if (idx == POINT_HT)
	{
		ch->PointChange(POINT_MAX_SP, 0);
	}

	ch->PointChange(POINT_STAT, -1);
	ch->ComputePoints();
}

ACMD(do_pvp)
{
	if (ch->GetArena() != NULL || CArenaManager::instance().IsArenaMap(ch->GetMapIndex()) == true)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER pkVictim = CHARACTER_MANAGER::instance().Find(vid);

	if (!pkVictim)
		return;

	if (pkVictim->IsNPC())
		return;

	// MT2009_PLUS_DIGI_SERVER_QOL_V1 (duel block): the messenger's block list stops a duel request both ways (Autor: Digi Rasta).
	{ bool Mt2009DigiBlocked(LPCHARACTER, LPCHARACTER); if (Mt2009DigiBlocked(ch, pkVictim)) return; }
	if (pkVictim->GetArena() != NULL)
	{
		pkVictim->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This player is currently fighting."));
		return;
	}

	CPVPManager::instance().Insert(ch, pkVictim);
}

ACMD(do_guildskillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	if (!ch->GetGuild())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] It does not belong to the guild."));
		return;
	}

	CGuild* g = ch->GetGuild();
	TGuildMember* gm = g->GetMember(ch->GetPlayerID());
	if (gm->grade == GUILD_LEADER_GRADE)
	{
		DWORD vnum = 0;
		str_to_number(vnum, arg1);
		g->SkillLevelUp(vnum);
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] You do not have the authority to change the level of the guild skills."));
	}
}

ACMD(do_skillup)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vnum = 0;
	str_to_number(vnum, arg1);

	if (true == ch->CanUseSkill(vnum))
	{
		ch->SkillLevelUp(vnum);
	}
	else
	{
		switch(vnum)
		{
			case SKILL_HORSE_WILDATTACK:
			case SKILL_HORSE_CHARGE:
			case SKILL_HORSE_ESCAPE:
			case SKILL_HORSE_WILDATTACK_RANGE:

			case SKILL_7_A_ANTI_TANHWAN:
			case SKILL_7_B_ANTI_AMSEOP:
			case SKILL_7_C_ANTI_SWAERYUNG:
			case SKILL_7_D_ANTI_YONGBI:

			case SKILL_8_A_ANTI_GIGONGCHAM:
			case SKILL_8_B_ANTI_YEONSA:
			case SKILL_8_C_ANTI_MAHWAN:
			case SKILL_8_D_ANTI_BYEURAK:

			case SKILL_ADD_HP:
			case SKILL_RESIST_PENETRATE:
				ch->SkillLevelUp(vnum);
				break;
		}
	}
}

//
//
ACMD(do_safebox_close)
{
	ch->CloseSafebox();
}

//
//
ACMD(do_safebox_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	ch->ReqSafeboxLoad(arg1);
}

ACMD(do_safebox_change_password)
{
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || strlen(arg1)>6)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Storeroom] You have entered an incorrect password."));
		return;
	}

	if (!*arg2 || strlen(arg2)>6)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Storeroom] You have entered an incorrect password."));
		return;
	}

	TSafeboxChangePasswordPacket p;

	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy(p.szOldPassword, arg1, sizeof(p.szOldPassword));
	strlcpy(p.szNewPassword, arg2, sizeof(p.szNewPassword));

	db_clientdesc->DBPacket(HEADER_GD_SAFEBOX_CHANGE_PASSWORD, ch->GetDesc()->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_password)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1 || strlen(arg1) > 6)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Storeroom] You have entered an incorrect password."));
		return;
	}

	int iPulse = thecore_pulse();

	if (g_bChannel > 90)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot do that on special channel."));
		return;
	}

	if (ch->GetMall())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Storeroom] The Storeroom is already open."));
		return;
	}

	if (iPulse - ch->GetMallLoadTime() < passes_per_sec * 10)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Storeroom] You have to wait 10 seconds before you can open the Storeroom again."));
		return;
	}

	ch->SetMallLoadTime(iPulse);

	TSafeboxLoadPacket p;
	p.dwID = ch->GetDesc()->GetAccountTable().id;
	strlcpy(p.szLogin, ch->GetDesc()->GetAccountTable().login, sizeof(p.szLogin));
	strlcpy(p.szPassword, arg1, sizeof(p.szPassword));

	db_clientdesc->DBPacket(HEADER_GD_MALL_LOAD, ch->GetDesc()->GetHandle(), &p, sizeof(p));
}

ACMD(do_mall_close)
{
	if (ch->GetMall())
	{
		ch->SetMallLoadTime(thecore_pulse());
		ch->CloseMall();
		ch->Save();
	}
}

ACMD(do_ungroup)
{
	if (!ch->GetParty())
		return;

	if (!CPartyManager::instance().IsEnablePCParty())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Group] The server cannot execute this group request."));
		return;
	}

	if (ch->GetDungeon())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Group] You cannot leave a group while you are in a dungeon."));
		return;
	}

	LPPARTY pParty = ch->GetParty();

	if (pParty->GetMemberCount() == 2)
	{
		// party disband
		CPartyManager::instance().DeleteParty(pParty);
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Group] You have left the group."));
		//pParty->SendPartyRemoveOneToAll(ch);
		pParty->Quit(ch->GetPlayerID());
		//pParty->SendPartyRemoveAllToOne(ch);
	}
}

ACMD(do_close_shop)
{
	if (ch->IsObserverMode())
		return;

	if (ch->IsBusy(BUSY_SHOP_MANAGE | BUSY_MYSHOP | BUSY_SHOP)) {
		ch->ChatDebug("you're busy");
		return;
	}

	if (ch->GetMyShop())
	{
		ch->CloseMyShop();
		return;
	}
}

ACMD(do_set_walk_mode)
{
	ch->SetNowWalking(true);
	ch->SetWalking(true);
}

ACMD(do_set_run_mode)
{
	ch->SetNowWalking(false);
	ch->SetWalking(false);
}

// MT2009_PLUS_GUILD_WAR_JOIN_V1 (command): the client's "Wejdz na wojne" button (the guild
// war board and the notice at the war's start, uiguild.py / game.py). The
// same entry as the guild_war_join letter's "Tak": the war map of an arena
// war, the guild's camp on the kingdom's guild map in a war on a bot guild.
ACMD(do_guild_war_enter)
{
	CGuild* g = ch->GetGuild();
	if (!g)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Nie nalezysz do gildii.");
		return;
	}
	const DWORD dwOppGID = g->UnderAnyWar();
	if (!dwOppGID)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Twoja gildia nie prowadzi teraz wojny.");
		return;
	}
	if (ch->GetWarMap())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Jestes juz na polu wojny.");
		return;
	}
	if (ch->IsDead() || !ch->CanWarp())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "[Wojna] Teraz nie mozesz sie przeniesc - sprobuj za chwile.");
		return;
	}
	g->GuildWarEntryAccept(dwOppGID, ch);
}

ACMD(do_war)
{
	CGuild * g = ch->GetGuild();

	if (!g)
		return;

	if (g->UnderAnyWar())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] Your guild is already participating in another war."));
		return;
	}

	char arg1[256], arg2[256];
	DWORD type = GUILD_WAR_TYPE_FIELD; //fixme102 base int modded uint
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
		return;

	if (*arg2)
	{
		str_to_number(type, arg2);

		if (type >= GUILD_WAR_TYPE_MAX_NUM)
			type = GUILD_WAR_TYPE_FIELD;
	}

	DWORD gm_pid = g->GetMasterPID();

	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] No one is entitled to a guild war."));
		return;
	}

	CGuild * opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] No guild with this name exists."));
		return;
	}

	// Playerbot: a war on a bot guild is a field war the bots answer
	// themselves (playerbotify.py, apply_player_war_on_bot_guilds).
	if (CPlayerBotManager::instance().OnPlayerWarRequest(ch, g, opp_g))
		return;

	switch (g->GetGuildWarState(opp_g->GetID()))
	{
		case GUILD_WAR_NONE:
			{
				if (opp_g->UnderAnyWar())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] This guild is already participating in another war."));
					return;
				}

				int iWarPrice = KOR_aGuildWarInfo[type].iWarPrice;

				if (g->GetGuildMoney() < iWarPrice)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] Not enough Yang to participate in a guild war."));
					return;
				}

				if (opp_g->GetGuildMoney() < iWarPrice)
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] The guild does not have enough Yang to participate in a guild war."));
					return;
				}
			}
			break;

		case GUILD_WAR_SEND_DECLARE:
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] This guild is already participating in a war."));
				return;
			}
			break;

		case GUILD_WAR_RECV_DECLARE:
			{
				if (opp_g->UnderAnyWar())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] This guild is already participating in another war."));
					g->RequestRefuseWar(opp_g->GetID());
					return;
				}

				// Playerbot: an answer is to the war that was declared. The
				// accept dialog sends "/war <guild>" with no type, so type is
				// still the field war here, which CanStartWar refuses on this
				// engine: no declaration could be accepted, and a guild that
				// met every other rule was refused without a word
				// (playerbotify.py, apply_guild_war_answer_type).
				type = g->GetGuildWarType(opp_g->GetID());
			}
			break;

		case GUILD_WAR_RESERVE:
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] This Guild is already scheduled for another war."));
				return;
			}
			break;

		case GUILD_WAR_END:
			return;

		default:
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] This guild is taking part in a battle at the moment."));
			g->RequestRefuseWar(opp_g->GetID());
			return;
	}

	if (!g->CanStartWar(type))
	{
		if (g->GetLadderPoint() == 0)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] The guild level is too low."));
			sys_log(0, "GuildWar.StartError.NEED_LADDER_POINT");
		}
		else if (g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] A minimum of %d players are needed to participate in a guild war."), GUILD_WAR_MIN_MEMBER_COUNT);
			sys_log(0, "GuildWar.StartError.NEED_MINIMUM_MEMBER[%d]", GUILD_WAR_MIN_MEMBER_COUNT);
		}
		else
		{
			sys_log(0, "GuildWar.StartError.UNKNOWN_ERROR");
		}
		return;
	}

	if (!opp_g->CanStartWar(type))
	{
		if (opp_g->GetLadderPoint() == 0)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] The guild does not have enough points to participate in a guild war."));
		else if (opp_g->GetMemberCount() < GUILD_WAR_MIN_MEMBER_COUNT)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] The guild does not have enough members to participate in a guild war."));
		return;
	}

	do
	{
		if (g->GetMasterCharacter() != NULL)
			break;

		CCI *pCCI = P2P_MANAGER::instance().FindByPID(g->GetMasterPID());

		if (pCCI != NULL)
			break;

		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] The enemy's guild leader is offline."));
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	do
	{
		if (opp_g->GetMasterCharacter() != NULL)
			break;

		CCI *pCCI = P2P_MANAGER::instance().FindByPID(opp_g->GetMasterPID());

		if (pCCI != NULL)
			break;

		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] The enemy's guild leader is offline."));
		g->RequestRefuseWar(opp_g->GetID());
		return;

	} while (false);

	g->RequestDeclareWar(opp_g->GetID(), type);
}

ACMD(do_nowar)
{
	CGuild* g = ch->GetGuild();
	if (!g)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD gm_pid = g->GetMasterPID();

	if (gm_pid != ch->GetPlayerID())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] No one is entitled to a guild war."));
		return;
	}

	CGuild* opp_g = CGuildManager::instance().FindGuildByName(arg1);

	if (!opp_g)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[Guild] No guild with this name exists."));
		return;
	}

	g->RequestRefuseWar(opp_g->GetID());
}

ACMD(do_detaillog)
{
	ch->DetailLog();
}

ACMD(do_monsterlog)
{
	ch->ToggleMonsterLog();
}

ACMD(do_pkmode)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	BYTE mode = 0;
	str_to_number(mode, arg1);

	if (mode == PK_MODE_PROTECT)
		return;

	if (ch->GetLevel() < PK_PROTECT_LEVEL && mode != PK_MODE_PROTECT)
		return;

	if (CWarMapManager::instance().IsWarMap(ch->GetMapIndex()) && mode != PK_MODE_PEACE)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change PvP mode on war."));
		mode = PK_MODE_PEACE;
	}

	ch->SetPKMode(mode);
}

ACMD(do_messenger_auth)
{
	if (ch->GetArena())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
		return;
	}

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1 || !*arg2)
		return;

	char answer = LOWER(*arg1);
	// @fixme130 AuthToAdd void -> bool
	bool bIsDenied = answer != 'y';
	bool bIsAdded = MessengerManager::instance().AuthToAdd(ch->GetName(), arg2, bIsDenied); // DENY
	if (bIsAdded && bIsDenied)
	{
		LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg2);

		if (tch)
			tch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s declined the invitation."), ch->GetName());
	}
}

ACMD(do_setblockmode)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		BYTE flag = 0;
		str_to_number(flag, arg1);
		ch->SetBlockMode(flag);
	}
}

// Refine QoL: "nie zamykaj okna". skipSave=false, bo wybor ma przezyc relog.
ACMD(do_refine_keep_open)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		BYTE flag = 0;
		str_to_number(flag, arg1);
		ch->SetSpecialFlag("refine.keep_open", flag ? 1 : 2, false);
	}
}

ACMD(do_unmount)
{
	if (true == ch->UnEquipSpecialRideUniqueItem())
	{
		ch->RemoveAffect(AFFECT_MOUNT);
		ch->RemoveAffect(AFFECT_MOUNT_BONUS);

		if (ch->IsHorseRiding())
		{
			ch->StopRiding();
		}
		else
		{
			ch->OnStopRiding();
		}
	}
	else
	{
		ch->ChatPacket( CHAT_TYPE_INFO, LC_TEXT("Your inventory is full."));
	}
}

ACMD(do_observer_exit)
{
	if (ch->IsObserverMode())
	{
		if (ch->GetWarMap())
			ch->SetWarMap(NULL);

		if (ch->GetArena() != NULL || ch->GetArenaObserverMode() == true)
		{
			ch->SetArenaObserverMode(false);

			if (ch->GetArena() != NULL)
				ch->GetArena()->RemoveObserver(ch->GetPlayerID());

			ch->SetArena(NULL);
			ch->WarpSet(ARENA_RETURN_POINT_X(ch->GetEmpire()), ARENA_RETURN_POINT_Y(ch->GetEmpire()));
		}
		else
		{
			ch->ExitToSavedLocation();
		}
		ch->SetObserverMode(false);
	}
}

ACMD(do_view_equip)
{
	if (ch->GetGMLevel() <= GM_PLAYER)
		return;

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (*arg1)
	{
		DWORD vid = 0;
		str_to_number(vid, arg1);
		LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

		if (!tch)
			return;

		if (!tch->IsPC())
			return;

		tch->SendEquipment(ch);
	}
}

ACMD(do_party_request)
{
	if (ch->GetArena())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
		return;
	}

	if (ch->GetParty())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot accept the invitation because you are already in the group."));
		return;
	}

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		if (!ch->RequestToParty(tch))
			ch->ChatPacket(CHAT_TYPE_COMMAND, "PartyRequestDenied");
}

ACMD(do_party_request_accept)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->AcceptToParty(tch);
}

ACMD(do_party_request_deny)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
		return;

	DWORD vid = 0;
	str_to_number(vid, arg1);
	LPCHARACTER tch = CHARACTER_MANAGER::instance().Find(vid);

	if (tch)
		ch->DenyToParty(tch);
}

ACMD(do_monarch_warpto)
{
	if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This function can only be used by the emperor."));
		return;
	}

	if (!ch->IsMCOK(CHARACTER::MI_WARP))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cooldown time for approximately %d seconds"), ch->GetMCLTime(CHARACTER::MI_WARP));
		return;
	}

	const int WarpPrice = 10000;

	if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Lack of Taxes. Current Capital : %u Needed Capital : %u"), NationMoney, WarpPrice);
		return;
	}

	int x = 0, y = 0;
	char arg1[256];

	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Command: warpto <character name>"));
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(arg1);

		if (pkCCI)
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("You cannot be warped to an unknown player."));
				return;
			}

			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Adding player %d into the channel. (Present channel %d)"), pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone(pkCCI->lMapIndex))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("You cannot move to that area."));
				return;
			}

			PIXEL_POSITION pos;

			if (!SECTREE_MANAGER::instance().GetCenterPositionOfMap(pkCCI->lMapIndex, pos))
				ch->ChatPacket(CHAT_TYPE_INFO, "Cannot find map (index %d)", pkCCI->lMapIndex);
			else
			{
				//ch->ChatPacket(CHAT_TYPE_INFO, "You warp to (%d, %d)", pos.x, pos.y);
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Warp to player %s."), arg1);
				ch->WarpSet(pos.x, pos.y);

				CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

				ch->SetMC(CHARACTER::MI_WARP);
			}
		}
		else if (NULL == CHARACTER_MANAGER::instance().FindPC(arg1))
		{
			ch->ChatPacket(CHAT_TYPE_INFO, "There is no one by that name");
		}

		return;
	}
	else
	{
		if (tch->GetEmpire() != ch->GetEmpire())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot be warped to an unknown player."));
			return;
		}
		if (!IsMonarchWarpZone(tch->GetMapIndex()))
		{
			ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("You cannot move to that area."));
			return;
		}
		x = tch->GetX();
		y = tch->GetY();
	}

	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Warp to player %s."), arg1);
	ch->WarpSet(x, y);
	ch->Stop();

	CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

	ch->SetMC(CHARACTER::MI_WARP);
}

ACMD(do_monarch_transfer)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Use: transfer <name>"));
		return;
	}

	if (!CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This function can only be used by the emperor."));
		return;
	}

	if (!ch->IsMCOK(CHARACTER::MI_TRANSFER))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cooldown time for approximately %d seconds"), ch->GetMCLTime(CHARACTER::MI_TRANSFER));
		return;
	}

	const int WarpPrice = 10000;

	if (!CMonarch::instance().IsMoneyOk(WarpPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Lack of Taxes. Current Capital : %u Needed Capital : %u"), NationMoney, WarpPrice);
		return;
	}

	LPCHARACTER tch = CHARACTER_MANAGER::instance().FindPC(arg1);

	if (!tch)
	{
		CCI * pkCCI = P2P_MANAGER::instance().Find(arg1);

		if (pkCCI)
		{
			if (pkCCI->bEmpire != ch->GetEmpire())
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot recruit players from another kingdom."));
				return;
			}
			if (pkCCI->bChannel != g_bChannel)
			{
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The player %s is on channel %d at the moment. (Your channel: %d)"), arg1, pkCCI->bChannel, g_bChannel);
				return;
			}
			if (!IsMonarchWarpZone(pkCCI->lMapIndex))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("You cannot move to that area."));
				return;
			}
			if (!IsMonarchWarpZone(ch->GetMapIndex()))
			{
				ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("You cannot summon to that area."));
				return;
			}

			TPacketGGTransfer pgg;

			pgg.bHeader = HEADER_GG_TRANSFER;
			strlcpy(pgg.szName, arg1, sizeof(pgg.szName));
			pgg.lX = ch->GetX();
			pgg.lY = ch->GetY();

			P2P_MANAGER::instance().Send(&pgg, sizeof(TPacketGGTransfer));
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have recruited %s players."), arg1);

			CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

			ch->SetMC(CHARACTER::MI_TRANSFER);
		}
		else
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is no user with this name."));
		}

		return;
	}

	if (ch == tch)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot recruit yourself."));
		return;
	}

	if (tch->GetEmpire() != ch->GetEmpire())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot recruit players from another kingdom."));
		return;
	}
	if (!IsMonarchWarpZone(tch->GetMapIndex()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("You cannot move to that area."));
		return;
	}
	if (!IsMonarchWarpZone(ch->GetMapIndex()))
	{
		ch->ChatPacket (CHAT_TYPE_INFO, LC_TEXT("You cannot summon to that area."));
		return;
	}

	//tch->Show(ch->GetMapIndex(), ch->GetX(), ch->GetY(), ch->GetZ());
	tch->WarpSet(ch->GetX(), ch->GetY(), ch->GetMapIndex());

	CMonarch::instance().SendtoDBDecMoney(WarpPrice, ch->GetEmpire(), ch);

	ch->SetMC(CHARACTER::MI_TRANSFER);
}

ACMD(do_monarch_info)
{
	if (CMonarch::instance().IsMonarch(ch->GetPlayerID(), ch->GetEmpire()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("My information about the emperor"));
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			if (n == ch->GetEmpire())
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[%sMonarch] : %s Yang owned %lld"), EMPIRE_NAME(n), p->name[n], p->money[n]);
			else
				ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[%sMonarch] : %s"), EMPIRE_NAME(n), p->name[n]);

		}
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Information about the emperor"));
		TMonarchInfo * p = CMonarch::instance().GetMonarch();
		for (int n = 1; n < 4; ++n)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("[%sMonarch] : %s"), EMPIRE_NAME(n), p->name[n]);

		}
	}
}

ACMD(do_elect)
{
	db_clientdesc->DBPacketHeader(HEADER_GD_COME_TO_VOTE, ch->GetDesc()->GetHandle(), 0);
}

// LUA_ADD_GOTO_INFO
struct GotoInfo
{
	std::string 	st_name;

	BYTE 	empire;
	int 	mapIndex;
	DWORD 	x, y;

	GotoInfo()
	{
		st_name 	= "";
		empire 		= 0;
		mapIndex 	= 0;

		x = 0;
		y = 0;
	}

	GotoInfo(const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void operator = (const GotoInfo& c_src)
	{
		__copy__(c_src);
	}

	void __copy__(const GotoInfo& c_src)
	{
		st_name 	= c_src.st_name;
		empire 		= c_src.empire;
		mapIndex 	= c_src.mapIndex;

		x = c_src.x;
		y = c_src.y;
	}
};

ACMD(do_monarch_tax)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: monarch_tax <1-50>");
		return;
	}

	if (!ch->IsMonarch())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Only an emperor can use this."));
		return;
	}

	int tax = 0;
	str_to_number(tax,  arg1);

	if (tax < 1 || tax > 50)
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Choose a number between 1 and 50."));

	quest::CQuestManager::instance().SetEventFlag("trade_tax", tax);

	ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Taxes are set to %d%%."));

	char szMsg[1024];

	snprintf(szMsg, sizeof(szMsg), "군주의 명으로 세금이 %d %% 로 변경되었습니다", tax);
	BroadcastNotice(szMsg);

	snprintf(szMsg, sizeof(szMsg), "앞으로는 거래 금액의 %d %% 가 국고로 들어가게됩니다.", tax);
	BroadcastNotice(szMsg);

	ch->SetMC(CHARACTER::MI_TAX);
}

static const DWORD cs_dwMonarchMobVnums[] =
{
	191,
	192,
	193,
	194,
	391,
	392,
	393,
	394,
	491,
	492,
	493,
	494,
	591,
	691,
	791,
	1304,
	1901,
	2091,
	2191,
	2206,
	0,
};

ACMD(do_monarch_mob)
{
	char arg1[256];
	LPCHARACTER	tch;

	one_argument(argument, arg1, sizeof(arg1));

	if (!ch->IsMonarch())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Only an emperor can use this."));
		return;
	}

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: mmob <mob name>");
		return;
	}

#ifdef ENABLE_MONARCH_MOB_CMD_MAP_CHECK // @warme006
	BYTE pcEmpire = ch->GetEmpire();
	BYTE mapEmpire = SECTREE_MANAGER::instance().GetEmpireFromMapIndex(ch->GetMapIndex());
	if (mapEmpire != pcEmpire && mapEmpire != 0)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can use this skill only in your land."));
		return;
	}
#endif

	const int SummonPrice = 5000000;

	if (!ch->IsMCOK(CHARACTER::MI_SUMMON))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cooldown time for approximately %d seconds"), ch->GetMCLTime(CHARACTER::MI_SUMMON));
		return;
	}

	if (!CMonarch::instance().IsMoneyOk(SummonPrice, ch->GetEmpire()))
	{
		int NationMoney = CMonarch::instance().GetMoney(ch->GetEmpire());
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Lack of Taxes. Current Capital : %u Needed Capital : %u"), NationMoney, SummonPrice);
		return;
	}

	const CMob * pkMob;
	DWORD vnum = 0;

	if (isdigit(*arg1))
	{
		str_to_number(vnum, arg1);

		if ((pkMob = CMobManager::instance().Get(vnum)) == NULL)
			vnum = 0;
	}
	else
	{
		pkMob = CMobManager::Instance().Get(arg1, true);

		if (pkMob)
			vnum = pkMob->m_table.dwVnum;
	}

	DWORD count;

	for (count = 0; cs_dwMonarchMobVnums[count] != 0; ++count)
		if (cs_dwMonarchMobVnums[count] == vnum)
			break;

	if (0 == cs_dwMonarchMobVnums[count])
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The Monster cannot be called. Check the Mob Number."));
		return;
	}

	tch = CHARACTER_MANAGER::instance().SpawnMobRange(vnum,
			ch->GetMapIndex(),
			ch->GetX() - number(200, 750),
			ch->GetY() - number(200, 750),
			ch->GetX() + number(200, 750),
			ch->GetY() + number(200, 750),
			true,
			pkMob->m_table.bType == CHAR_TYPE_STONE,
			true);

	if (tch)
	{
		CMonarch::instance().SendtoDBDecMoney(SummonPrice, ch->GetEmpire(), ch);

		ch->SetMC(CHARACTER::MI_SUMMON);
	}
}

static const char* FN_point_string(int apply_number)
{
	switch (apply_number)
	{
		case POINT_MAX_HP:	return LC_TEXT("Hit Points +%d");
		case POINT_MAX_SP:	return LC_TEXT("Spell Points +%d");
		case POINT_HT:		return LC_TEXT("Endurance +%d");
		case POINT_IQ:		return LC_TEXT("Intelligence +%d");
		case POINT_ST:		return LC_TEXT("Strength +%d");
		case POINT_DX:		return LC_TEXT("Dexterity +%d");
		case POINT_ATT_SPEED:	return LC_TEXT("Attack Speed +%d");
		case POINT_MOV_SPEED:	return LC_TEXT("Movement Speed %d");
		case POINT_CASTING_SPEED:	return LC_TEXT("Cooldown Time -%d");
		case POINT_HP_REGEN:	return LC_TEXT("Energy Recovery +%d");
		case POINT_SP_REGEN:	return LC_TEXT("Spell Point Recovery +%d");
		case POINT_POISON_PCT:	return LC_TEXT("Poison Attack %d");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_BLEEDING_PCT:	return LC_TEXT("Poison Attack %d");
#endif
		case POINT_STUN_PCT:	return LC_TEXT("Star +%d");
		case POINT_SLOW_PCT:	return LC_TEXT("Speed Reduction +%d");
		case POINT_CRITICAL_PCT:	return LC_TEXT("Critical Attack with a chance of %d%%");
		case POINT_RESIST_CRITICAL:	return LC_TEXT("상대의 치명타 확률 %d%% 감소");
		case POINT_PENETRATE_PCT:	return LC_TEXT("Chance of a Speared Attack of %d%%");
		case POINT_RESIST_PENETRATE: return LC_TEXT("상대의 관통 공격 확률 %d%% 감소");
		case POINT_ATTBONUS_HUMAN:	return LC_TEXT("Player's Attack Power against Monsters +%d%%");
		case POINT_ATTBONUS_ANIMAL:	return LC_TEXT("Horse's Attack Power against Monsters +%d%%");
		case POINT_ATTBONUS_ORC:	return LC_TEXT("Attack Boost against Wonggui + %d%%");
		case POINT_ATTBONUS_MILGYO:	return LC_TEXT("Attack Boost against Milgyo + %d%%");
		case POINT_ATTBONUS_UNDEAD:	return LC_TEXT("Attack boost against zombies + %d%%");
		case POINT_ATTBONUS_DEVIL:	return LC_TEXT("Attack boost against devils + %d%%");
		case POINT_STEAL_HP:		return LC_TEXT("Absorbing of Energy %d%% while attacking.");
		case POINT_STEAL_SP:		return LC_TEXT("Absorption of Spell Points (SP) %d%% while attacking.");
		case POINT_MANA_BURN_PCT:	return LC_TEXT("With a chance of %d%% Spell Points (SP) will be taken from the enemy.");
		case POINT_DAMAGE_SP_RECOVER:	return LC_TEXT("Absorbing of Spell Points (SP) with a chance of %d%%");
		case POINT_BLOCK:			return LC_TEXT("%d%% Chance of blocking a close-combat attack");
		case POINT_DODGE:			return LC_TEXT("%d%% Chance of blocking a long range attack");
		case POINT_RESIST_SWORD:	return LC_TEXT("One-Handed Sword defence %d%%");
		case POINT_RESIST_TWOHAND:	return LC_TEXT("Two-Handed Sword Defence %d%%");
		case POINT_RESIST_DAGGER:	return LC_TEXT("Two-Handed Sword Defence %d%%");
		case POINT_RESIST_BELL:		return LC_TEXT("Bell Defence %d%%");
		case POINT_RESIST_FAN:		return LC_TEXT("Fan Defence %d%%");
		case POINT_RESIST_BOW:		return LC_TEXT("Distant Attack Resistance %d%%");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_RESIST_CLAW:		return LC_TEXT("Two-Handed Sword Defence %d%%");
#endif
		case POINT_RESIST_FIRE:		return LC_TEXT("Fire Resistance %d%%");
		case POINT_RESIST_ELEC:		return LC_TEXT("Lightning Resistance %d%%");
		case POINT_RESIST_MAGIC:	return LC_TEXT("Magic Resistance %d%%");
#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
		case POINT_RESIST_MAGIC_REDUCTION:	return LC_TEXT("Magic Resistance %d%%");
#endif
		case POINT_RESIST_WIND:		return LC_TEXT("Wind Resistance %d%%");
		case POINT_RESIST_ICE:		return LC_TEXT("냉기 저항 %d%%");
		case POINT_RESIST_EARTH:	return LC_TEXT("대지 저항 %d%%");
		case POINT_RESIST_DARK:		return LC_TEXT("어둠 저항 %d%%");
		case POINT_REFLECT_MELEE:	return LC_TEXT("Reflect Direct Hit: %d%%");
		case POINT_POISON_REDUCE:	return LC_TEXT("Poison Resistance %d%%");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_BLEEDING_REDUCE:	return LC_TEXT("Poison Resistance %d%%");
#endif
		case POINT_KILL_SP_RECOVER:	return LC_TEXT("Spell Points (SP) will be increased by %d%% if you win.");
		case POINT_EXP_DOUBLE_BONUS:	return LC_TEXT("Experience increases by %d%% if you win against an opponent.");
		case POINT_GOLD_DOUBLE_BONUS:	return LC_TEXT("Increase of Yang up to %d%% if you win.");
		case POINT_ITEM_DROP_BONUS:	return LC_TEXT("Increase of captured Items up to %d%% if you win.");
		case POINT_POTION_BONUS:	return LC_TEXT("Power increase of up to %d%% after taking the potion.");
		case POINT_KILL_HP_RECOVERY:	return LC_TEXT("%d%% Chance of filling up Hit Points after a victory.");
		case POINT_ATT_GRADE_BONUS:	return LC_TEXT("Attack Power + %d");
		case POINT_DEF_GRADE_BONUS:	return LC_TEXT("Armour + %d");
		case POINT_MAGIC_ATT_GRADE:	return LC_TEXT("Magical Attack + %d");
		case POINT_MAGIC_DEF_GRADE:	return LC_TEXT("Magical Defence + %d");
		case POINT_MAX_STAMINA:	return LC_TEXT("Maximum Endurance + %d");
		case POINT_ATTBONUS_WARRIOR:	return LC_TEXT("Strong against Warriors + %d%%");
		case POINT_ATTBONUS_ASSASSIN:	return LC_TEXT("Strong against Ninjas + %d%%");
		case POINT_ATTBONUS_SURA:		return LC_TEXT("Strong against Sura + %d%%");
		case POINT_ATTBONUS_SHAMAN:		return LC_TEXT("Strong against Shamans + %d%%");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_ATTBONUS_WOLFMAN:	return LC_TEXT("Strong against Shamans + %d%%");
#endif
		case POINT_ATTBONUS_MONSTER:	return LC_TEXT("Strength against monsters + %d%%");
		case POINT_MALL_ATTBONUS:		return LC_TEXT("Attack + %d%%");
		case POINT_MALL_DEFBONUS:		return LC_TEXT("Defence + %d%%");
		case POINT_MALL_EXPBONUS:		return LC_TEXT("Experience %d%%");
		case POINT_MALL_ITEMBONUS:		return LC_TEXT("아이템 드롭율 %d배"); // @fixme180 float to int
		case POINT_MALL_GOLDBONUS:		return LC_TEXT("돈 드롭율 %d배"); // @fixme180 float to int
		case POINT_MAX_HP_PCT:			return LC_TEXT("Maximum Energy +%d%%");
		case POINT_MAX_SP_PCT:			return LC_TEXT("Maximum Energy +%d%%");
		case POINT_SKILL_DAMAGE_BONUS:	return LC_TEXT("Skill Damage %d%%");
		case POINT_NORMAL_HIT_DAMAGE_BONUS:	return LC_TEXT("Hit Damage %d%%");
		case POINT_SKILL_DEFEND_BONUS:		return LC_TEXT("Resistance against Skill Damage %d%%");
		case POINT_NORMAL_HIT_DEFEND_BONUS:	return LC_TEXT("Resistance against Hits %d%%");
		case POINT_RESIST_WARRIOR:	return LC_TEXT("%d%% Resistance against Warrior Attacks");
		case POINT_RESIST_ASSASSIN:	return LC_TEXT("%d%% Resistance against Ninja Attacks");
		case POINT_RESIST_SURA:		return LC_TEXT("%d%% Resistance against Sura Attacks");
		case POINT_RESIST_SHAMAN:	return LC_TEXT("%d%% Resistance against Shaman Attacks");
#ifdef ENABLE_WOLFMAN_CHARACTER
		case POINT_RESIST_WOLFMAN:	return LC_TEXT("%d%% Resistance against Shaman Attacks");
#endif
		default:					return "UNK_ID %d%%"; // @fixme180
	}
}

static bool FN_hair_affect_string(LPCHARACTER ch, char *buf, size_t bufsiz)
{
	if (NULL == ch || NULL == buf)
		return false;

	CAffect* aff = NULL;
	time_t expire = 0;
	struct tm ltm;
	int	year, mon, day;
	int	offset = 0;

	aff = ch->FindAffect(AFFECT_HAIR);

	if (NULL == aff)
		return false;

	expire = ch->GetQuestFlag("hair.limit_time");

	if (expire < get_global_time())
		return false;

	// set apply string
	offset = snprintf(buf, bufsiz, FN_point_string(aff->bApplyOn), aff->lApplyValue);

	if (offset < 0 || offset >= (int) bufsiz)
		offset = bufsiz - 1;

	localtime_r(&expire, &ltm);

	year	= ltm.tm_year + 1900;
	mon		= ltm.tm_mon + 1;
	day		= ltm.tm_mday;

	snprintf(buf + offset, bufsiz - offset, LC_TEXT("(Procedure: %d y- %d m - %d d)"), year, mon, day);

	return true;
}

ACMD(do_costume)
{
	char buf[1024]; // @warme015
	const size_t bufferSize = sizeof(buf);

	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));

	CItem* pBody = ch->GetWear(WEAR_COSTUME_BODY);
	CItem* pHair = ch->GetWear(WEAR_COSTUME_HAIR);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	CItem* pMount = ch->GetWear(WEAR_COSTUME_MOUNT);
#endif
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	CItem* pAcce = ch->GetWear(WEAR_COSTUME_ACCE);
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	CItem* pWeapon = ch->GetWear(WEAR_COSTUME_WEAPON);
#endif

	ch->ChatPacket(CHAT_TYPE_INFO, "COSTUME status:");

	if (pHair)
	{
		const char* itemName = pHair->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  HAIR : %s", itemName);

		for (int i = 0; i < pHair->GetAttributeCount(); ++i)
		{
			const TPlayerItemAttribute& attr = pHair->GetAttribute(i);
			if (0 < attr.bType)
			{
				snprintf(buf, bufferSize, FN_point_string(attr.bType), attr.sValue);
				ch->ChatPacket(CHAT_TYPE_INFO, "     %s", buf);
			}
		}

		if (pHair->IsEquipped() && arg1[0] == 'h')
			ch->UnequipItem(pHair);
	}

	if (pBody)
	{
		const char* itemName = pBody->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  BODY : %s", itemName);

		if (pBody->IsEquipped() && arg1[0] == 'b')
			ch->UnequipItem(pBody);
	}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (pMount)
	{
		const char* itemName = pMount->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  MOUNT : %s", itemName);

		if (pMount->IsEquipped() && arg1[0] == 'm')
			ch->UnequipItem(pMount);
	}
#endif

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	if (pAcce)
	{
		const char* itemName = pAcce->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  ACCE : %s", itemName);

		if (pAcce->IsEquipped() && arg1[0] == 'a')
			ch->UnequipItem(pAcce);
	}
#endif

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	if (pWeapon)
	{
		const char* itemName = pWeapon->GetName();
		ch->ChatPacket(CHAT_TYPE_INFO, "  WEAPON : %s", itemName);

		if (pWeapon->IsEquipped() && arg1[0] == 'w')
			ch->UnequipItem(pWeapon);
	}
#endif
}

ACMD(do_hair)
{
	char buf[256];

	if (false == FN_hair_affect_string(ch, buf, sizeof(buf)))
		return;

	ch->ChatPacket(CHAT_TYPE_INFO, buf);
}

ACMD(do_inventory)
{
	int	index = 0;
	int	count		= 1;

	char arg1[256];
	char arg2[256];

	LPITEM	item;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!*arg1)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: inventory <start_index> <count>");
		return;
	}

	if (!*arg2)
	{
		index = 0;
		str_to_number(count, arg1);
	}
	else
	{
		str_to_number(index, arg1); index = MIN(index, INVENTORY_MAX_NUM);
		str_to_number(count, arg2); count = MIN(count, INVENTORY_MAX_NUM);
	}

	for (int i = 0; i < count; ++i)
	{
		if (index >= INVENTORY_MAX_NUM)
			break;

		item = ch->GetInventoryItem(index);

		ch->ChatPacket(CHAT_TYPE_INFO, "inventory [%d] = %s",
						index, item ? item->GetName() : "<NONE>");
		++index;
	}
}

//gift notify quest command
ACMD(do_gift)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "gift");
}

ACMD(do_cube)
{
	if (!ch->CanDoCube())
		return;

	sys_log(1, "CUBE COMMAND <%s>: %s", ch->GetName(), argument);
	int cube_index = 0, inven_index = 0;
	const char *line;

	char arg1[256], arg2[256], arg3[256];

	line = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(line, arg3, sizeof(arg3));

	if (0 == arg1[0])
	{
		// print usage
		ch->ChatPacket(CHAT_TYPE_INFO, "Usage: cube open");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube close");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube add <inveltory_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube delete <cube_index>");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube list");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube cancel");
		ch->ChatPacket(CHAT_TYPE_INFO, "       cube make [all]");
		return;
	}

	const std::string& strArg1 = std::string(arg1);

	// r_info (request information)

	//					    (Server -> Client) /cube r_list npcVNUM resultCOUNT 123,1/125,1/128,1/130,5

	//					   (Server -> Client) /cube m_info startIndex count 125,1|126,2|127,2|123,5&555,5&555,4/120000@125,1|126,2|127,2|123,5&555,5&555,4/120000

	if (strArg1 == "r_info")
	{
		if (0 == arg2[0])
			Cube_request_result_list(ch);
		else
		{
			if (isdigit(*arg2))
			{
				int listIndex = 0, requestCount = 1;
				str_to_number(listIndex, arg2);

				if (0 != arg3[0] && isdigit(*arg3))
					str_to_number(requestCount, arg3);

				Cube_request_material_info(ch, listIndex, requestCount);
			}
		}

		return;
	}

	switch (LOWER(arg1[0]))
	{
		case 'o':	// open
			Cube_open(ch);
			break;

		case 'c':	// close
			Cube_close(ch);
			break;

		case 'l':	// list
			Cube_show_list(ch);
			break;

		case 'a':	// add cue_index inven_index
			{
				if (0 == arg2[0] || !isdigit(*arg2) ||
					0 == arg3[0] || !isdigit(*arg3))
					return;

				str_to_number(cube_index, arg2);
				str_to_number(inven_index, arg3);
				Cube_add_item (ch, cube_index, inven_index);
			}
			break;

		case 'd':	// delete
			{
				if (0 == arg2[0] || !isdigit(*arg2))
					return;

				str_to_number(cube_index, arg2);
				Cube_delete_item (ch, cube_index);
			}
			break;

		case 'm':	// make
			if (0 != arg2[0])
			{
				while (true == Cube_make(ch))
					sys_log(1, "cube make success");
			}
			else
				Cube_make(ch);
			break;

		default:
			return;
	}
}

ACMD(do_in_game_mall)
{
	if (LC_IsEurope() == true)
	{
		// Uninitialised on every locale the switch below does not name; the
		// shop reads the code only as a language hint, so "en" is the fallback.
		char country_code[3] = "en";

		switch (LC_GetLocalType())
		{
			case LC_GERMANY:	country_code[0] = 'd'; country_code[1] = 'e'; country_code[2] = '\0'; break;
			case LC_FRANCE:		country_code[0] = 'f'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_ITALY:		country_code[0] = 'i'; country_code[1] = 't'; country_code[2] = '\0'; break;
			case LC_SPAIN:		country_code[0] = 'e'; country_code[1] = 's'; country_code[2] = '\0'; break;
			case LC_UK:			country_code[0] = 'e'; country_code[1] = 'n'; country_code[2] = '\0'; break;
			case LC_TURKEY:		country_code[0] = 't'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_POLAND:		country_code[0] = 'p'; country_code[1] = 'l'; country_code[2] = '\0'; break;
			case LC_PORTUGAL:	country_code[0] = 'p'; country_code[1] = 't'; country_code[2] = '\0'; break;
			case LC_GREEK:		country_code[0] = 'g'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_RUSSIA:		country_code[0] = 'r'; country_code[1] = 'u'; country_code[2] = '\0'; break;
			case LC_DENMARK:	country_code[0] = 'd'; country_code[1] = 'k'; country_code[2] = '\0'; break;
			case LC_BULGARIA:	country_code[0] = 'b'; country_code[1] = 'g'; country_code[2] = '\0'; break;
			case LC_CROATIA:	country_code[0] = 'h'; country_code[1] = 'r'; country_code[2] = '\0'; break;
			case LC_MEXICO:		country_code[0] = 'm'; country_code[1] = 'x'; country_code[2] = '\0'; break;
			case LC_ARABIA:		country_code[0] = 'a'; country_code[1] = 'e'; country_code[2] = '\0'; break;
			case LC_CZECH:		country_code[0] = 'c'; country_code[1] = 'z'; country_code[2] = '\0'; break;
			case LC_ROMANIA:	country_code[0] = 'r'; country_code[1] = 'o'; country_code[2] = '\0'; break;
			case LC_HUNGARY:	country_code[0] = 'h'; country_code[1] = 'u'; country_code[2] = '\0'; break;
			case LC_NETHERLANDS: country_code[0] = 'n'; country_code[1] = 'l'; country_code[2] = '\0'; break;
			case LC_USA:		country_code[0] = 'u'; country_code[1] = 's'; country_code[2] = '\0'; break;
			case LC_CANADA:	country_code[0] = 'c'; country_code[1] = 'a'; country_code[2] = '\0'; break;
			default:
				if (test_server == true)
				{
					country_code[0] = 'd'; country_code[1] = 'e'; country_code[2] = '\0';
				}
				break;
		}

		char buf[512+1];
		char sas[33];
		MD5_CTX ctx;
		const char sas_key[] = "GF9001";

		snprintf(buf, sizeof(buf), "%u%u%s", ch->GetPlayerID(), ch->GetAID(), sas_key);

		MD5Init(&ctx);
		MD5Update(&ctx, (const unsigned char *) buf, strlen(buf));
#ifdef __FreeBSD__
		MD5End(&ctx, sas);
#else
		static const char hex[] = "0123456789abcdef";
		unsigned char digest[16];
		MD5Final(digest, &ctx);
		int i;
		for (i = 0; i < 16; ++i) {
			sas[i+i] = hex[digest[i] >> 4];
			sas[i+i+1] = hex[digest[i] & 0x0f];
		}
		sas[i+i] = '\0';
#endif

		snprintf(buf, sizeof(buf), "mall http://%s/ishop?pid=%u&c=%s&sid=%d&sas=%s",
				g_strWebMallURL.c_str(), ch->GetPlayerID(), country_code, g_server_id, sas);

		ch->ChatPacket(CHAT_TYPE_COMMAND, buf);
	}
}

ACMD(do_dice)
{
	char arg1[256], arg2[256];
	int start = 1, end = 100;

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (*arg1 && *arg2)
	{
		start = atoi(arg1);
		end = atoi(arg2);
	}
	else if (*arg1 && !*arg2)
	{
		start = 1;
		end = atoi(arg1);
	}

	start = MAX(start, 1);
	end = MAX(end, 1);
	start = MIN(start, 100000);
	end = MIN(end, 100000);

	end = MAX(start, end);
	start = MIN(start, end);

	int n = number(start, end);

#ifdef ENABLE_DICE_SYSTEM
	if (ch->GetParty())
		ch->GetParty()->ChatPacketToAllMember(CHAT_TYPE_DICE_INFO, LC_TEXT("%s rolled dice and got %d. (%d-%d)"), ch->GetName(), n, start, end);
	else
		ch->ChatPacket(CHAT_TYPE_DICE_INFO, LC_TEXT("You rolled dice and got %d. (%d-%d)"), n, start, end);
#else
	if (ch->GetParty())
		ch->GetParty()->ChatPacketToAllMember(CHAT_TYPE_INFO, LC_TEXT("%s rolled dice and got %d. (%d-%d)"), ch->GetName(), n, start, end);
	else
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You rolled dice and got %d. (%d-%d)"), n, start, end);
#endif
}

#if defined(ENABLE_CHEQUE_SYSTEM) && defined(ENABLE_WON_EXCHANGE_WINDOW)
ACMD(do_won_exchange)
{
	char arg1[256];
	char arg2[256];

	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	if (!*arg1 || !*arg2)
		return;

	int won = 0;
	str_to_number(won, arg2);

	ch->WonExchange(arg1, won);
}
#endif

#ifdef ENABLE_NEWSTUFF
ACMD(do_click_safebox)
{
	if ((ch->GetGMLevel() <= GM_PLAYER) && (ch->GetDungeon() || ch->GetWarMap()))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot open the safebox in dungeon or at war."));
		return;
	}

	ch->SetSafeboxOpenPosition();
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ShowMeSafeboxPassword");
}
ACMD(do_force_logout)
{
	LPDESC pDesc=DESC_MANAGER::instance().FindByCharacterName(ch->GetName());
	if (!pDesc)
		return;
	pDesc->DelayedDisconnect(0);
}
#endif

ACMD(do_click_mall)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ShowMeMallPassword");
}

ACMD(do_ride)
{
    sys_log(1, "[DO_RIDE] start");
    if (ch->IsDead() || ch->IsStun() || ch->IsBusyAction())
	return;

    {
		if (ch->IsHorseRiding())
		{
			sys_log(1, "[DO_RIDE] stop riding");
			ch->StopRiding();
			return;
		}

		if (ch->GetMountVnum())
		{
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
			// MT2009_PLUS_MOUNT_QUICKSWAP_V1 (dismount), server-patches/mountquickswap:
			// Ctrl+G gets off a seal mount the way it gets off a horse - the
			// seal stays worn and the mount stands beside the rider again
			// (CMountSystem::Unmount, what the bots already do), so the next
			// Ctrl+G is back in the saddle at once. Before, do_unmount took the
			// seal off into the bag and getting on again meant equipping it,
			// which EquipItem refuses for 1.5 s after any attack or skill
			// ("You have to stand still to equip the item.") - get off, cast
			// an aura, and the mount would not take you back. The owner,
			// 3 October: "aby dzialal jak konie". A quest mount
			// (pc.set_mount) or an old ride item keeps the old way below.
			LPITEM pkWornSeal = ch->GetWear(WEAR_COSTUME_MOUNT);
			CMountSystem* pkMounts = ch->GetMountSystem();
			if (pkWornSeal && pkMounts && (DWORD)pkWornSeal->GetValue(1) == ch->GetMountVnum() &&
					pkMounts->GetByVnum(ch->GetMountVnum()))
			{
				// the horse's own refusal (StopRiding): not with a trade, a shop
				// or the storeroom open
				if (ch->IsBusy())
				{
					ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_CONTINUE_WHEN_BUSY"));
					return;
				}

				sys_log(1, "[DO_RIDE] get off the seal mount, the seal stays worn");
				pkMounts->Unmount(ch->GetMountVnum());
				// the movement check's allowance for a rider who gets off in a
				// run, as do_unmount and StopRiding give it
				ch->OnStopRiding();
				return;
			}
#endif
			sys_log(1, "[DO_RIDE] unmount");
			do_unmount(ch, NULL, 0, 0);
			return;
		}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		// A worn mount seal only spawns its following actor on equip
		// (item.cpp -> CHARACTER::MountSummon); nothing above this point ever
		// calls CMountSystem::Mount() to actually set AFFECT_MOUNT, so
		// GetMountVnum() above is always 0 here even while riding-eligible.
		// This is the missing "actually get on" step -- without it, Ctrl+G
		// falls through to the inventory loop below, which does not see an
		// equipped item and either does nothing or (if another copy of the
		// seal is sitting in the bag) equips that one instead, which looks
		// like the mount "re-summoning".
		if (LPITEM mountItem = ch->GetWear(WEAR_COSTUME_MOUNT))
		{
			if (ch->IsPolymorphed())
				return;

			CMountSystem* mountSystem = ch->GetMountSystem();
			DWORD mobVnum = mountItem->GetValue(1);

			// MT2009_PLUS_MOUNT_QUICKSWAP_V1 (mount): a worn seal whose mount is not
			// standing beside the rider (it was sent away, the rider came
			// back to life) is called first, so Ctrl+G is never a dead key;
			// MountSummon keeps its own refusals (OX map, arena).
			if (mountSystem && mobVnum && mountSystem->CountSummoned() == 0)
				ch->MountSummon(mountItem);

			if (mountSystem && mobVnum && mountSystem->CountSummoned() == 1)
			{
				// the horse's own pace: 1 s between two mounts (ePulse::HorseUse,
				// the clock do_ride and do_user_horse_ride use for a horse)
				if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::HorseUse, std::chrono::seconds(1)))
					return;

				sys_log(1, "[DO_RIDE] mount");
				mountSystem->Mount(mobVnum, mountItem);
			}
			return;
		}
#endif

		if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::HorseUse, std::chrono::seconds(1)))
		{
			return;
		}
    }

    {
	if (ch->GetHorse() != NULL)
	{
	    sys_log(1, "[DO_RIDE] start riding");
	    ch->StartRiding();
	    return;
	}

	for (BYTE i=0; i<INVENTORY_DEFAULT_MAX_NUM; ++i)
	{
	    LPITEM item = ch->GetInventoryItem(i);
	    if (NULL == item)
			continue;

		if (item->IsRideItem())
		{
			if (
				NULL==ch->GetWear(WEAR_UNIQUE1)
				|| NULL==ch->GetWear(WEAR_UNIQUE2)
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
				|| NULL==ch->GetWear(WEAR_COSTUME_MOUNT)
#endif
			)
			{
				sys_log(1, "[DO_RIDE] USE UNIQUE ITEM");
				//ch->EquipItem(item);
				ch->UseItem(TItemPos (INVENTORY, i));

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
				// A mount seal wasn't worn yet, so the toggle above just
				// equipped it (item.cpp -> CHARACTER::MountSummon, spawns
				// the following actor synchronously) -- without this, the
				// operator has to press Ctrl+G a second time to actually
				// get on, since the first press only got this far. Finish
				// the job in the same keypress.
				// MT2009_PLUS_MOUNT_QUICKSWAP_V1 (same keypress): EquipItem has already put a
				// player in the saddle (MT2009_PLUS_MOUNT_ON_EQUIP_V1); a second
				// Mount took him off and on again - two re-sends of the rider
				// to everyone around in one tick.
				if (item->IsNewMountItem() && item->IsEquipped() && !ch->GetMountVnum())
				{
					CMountSystem* mountSystem = ch->GetMountSystem();
					DWORD mobVnum = item->GetValue(1);

					if (mountSystem && mobVnum && mountSystem->CountSummoned() == 1)
					{
						sys_log(1, "[DO_RIDE] mount (same keypress)");
						mountSystem->Mount(mobVnum, item);
					}
				}
#endif
				return;
			}
		}

	    switch (item->GetVnum())
	    {
		case 71114:
		case 71116:
		case 71118:
		case 71120:
		    sys_log(1, "[DO_RIDE] USE QUEST ITEM");
		    ch->UseItem(TItemPos (INVENTORY, i));
		    return;
	    }

		if( (item->GetVnum() > 52000) && (item->GetVnum() < 52091) )	{
			sys_log(1, "[DO_RIDE] USE QUEST ITEM");
			ch->UseItem(TItemPos (INVENTORY, i));
		    return;
		}
	}
    }

    ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Please call your Horse first."));
}

#ifdef ENABLE_MOVE_CHANNEL
ACMD(DoChangeChannel)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	if (!*arg1)
		return;

	WORD channel = 0;
	str_to_number(channel, arg1);
	if (!channel)
		return;

	if (ch->m_pkTimedEvent)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Moving channel has been interrupted."));
		event_cancel(&ch->m_pkTimedEvent);
		return;
	}

	if (ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_WHILE_DEAD"));
		return;
	}

	if (channel == g_bChannel
		|| g_bAuthServer
		|| g_bChannel == 99
		|| ch->GetMapIndex() >= 1000
		|| ch->GetDungeon()
		|| ch->CanWarp() == false
		|| ch->IsBusy()
		)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change channel."));
		return;
	}

	ch->CreateMoveChannelEvent(channel);
}
#endif

ACMD(do_escape)
{
	if (!ch->HasPlayerData())
		return;

	if (ch->IsBusy())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_CONTINUE_WHEN_BUSY"));
		return;
	}

	if (ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_WHILE_DEAD"));
		return;
	}

	if (ch->IsPolymorphed())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change your status while you are transformed."));
		return;
	}

	if (get_dword_time() - ch->GetLastAttackTime() < 5000)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have to wait a while after combat before using this option."));
		return;
	}

	if (ch->playerData->GetEscapeCooltime() > thecore_pulse())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have to wait %d minutes to use this option again."), 1);
		return;
	}

	const BYTE bEmpire = ch->GetEmpire();
	const long lMapIndex = ch->GetMapIndex();
	const PIXEL_POSITION& rkPos = ch->GetXYZ();

	const LPSECTREE_MAP pSectreeMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);
	if (pSectreeMap == nullptr)
	{
		ch->WarpSet(g_start_position[bEmpire][0], g_start_position[bEmpire][1]);
		return;
	}

	const LPSECTREE pSectree = pSectreeMap->Find(rkPos.x, rkPos.y);
	const DWORD dwAttr = pSectree->GetAttribute(rkPos.x, rkPos.y);

	int iEscapeDistance = quest::CQuestManager::instance().GetEventFlag("escape_distance");
	iEscapeDistance = (iEscapeDistance > 0) ? iEscapeDistance : 300;

	int iEscapeCooltime = quest::CQuestManager::instance().GetEventFlag("escape_cooltime");
	iEscapeCooltime = (iEscapeCooltime > 0) ? iEscapeCooltime : 60;

	if (IS_SET(dwAttr, ATTR_BLOCK /*| ATTR_OBJECT*/))
	{
		/*
		* NOTE : If an object doesn't have a blocked area, players can still be blocked if they get inside it.
		* The problem is that bridges are treated as objects too, and we don't want players to use the escape feature through them.
		* The tricky part is figuring out whether a specific object is a bridge or not.
		* In the current state, we are only checking blocked areas.
		*
		* 2021.01.17.Owsap
		*/

		PIXEL_POSITION kNewPos;
		if (SECTREE_MANAGER::instance().GetRandomLocation(lMapIndex, kNewPos, rkPos.x, rkPos.y, iEscapeDistance))
		{
			char szBuf[255 + 1];
			snprintf(szBuf, sizeof(szBuf), "%ld, (%d, %d) -> (%d, %d)",
				lMapIndex, rkPos.x, rkPos.y, kNewPos.x, kNewPos.y);
			LogManager::instance().CharLog(ch, lMapIndex, "ESCAPE", szBuf);

			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have successfully freed yourself."));
			ch->Show(lMapIndex, kNewPos.x, kNewPos.y, rkPos.z);
		}
		else
		{
			char szBuf[255 + 1];
			snprintf(szBuf, sizeof(szBuf), "%ld, (%d, %d) -> EMPIRE START POSITION",
				lMapIndex, rkPos.x, rkPos.y);
			LogManager::instance().CharLog(ch, lMapIndex, "ESCAPE", szBuf);

			ch->WarpSet(g_start_position[bEmpire][0], g_start_position[bEmpire][1]);
		}
	}
	else
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are not stuck."));
	}

	ch->playerData->SetEscapeCooltime(thecore_pulse() + PASSES_PER_SEC(iEscapeCooltime));
}

ACMD(do_report_player)
{
	if (!PulseManager::Instance().IncreaseClock(ch->GetPlayerID(), ePulse::ReportPlayerCmd, std::chrono::milliseconds(3000)))
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("NEXT_ACTION_WAIT_NO_FORMAT"));
		return;
	}

	char arg1[256], arg2[256];
	two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));

	if (!isdigit(*arg1) || !isdigit(*arg2))
		return;

	DWORD vid = 0;
	BYTE reason = 0;

	str_to_number(vid, arg1);
	str_to_number(reason, arg2);

	if (vid == ch->GetVID())
		return;

	if (reason > 4)
		return;

	if (LPCHARACTER target = CHARACTER_MANAGER::instance().Find(vid))
	{
		if (!target->IsPC())
			return;

		if (!PulseManager::Instance().CheckClock(target->GetPlayerID(), ePulse::ReportPlayer))
		{
			ch->ChatDebug("Player was reported recently.");
			return;
		}

		ch->ChatDebug("Report player (reason %d)!", reason);
		LogManager::instance().ReportPlayerLog(ch, target, reason);
		PulseManager::Instance().IncreaseClock(target->GetPlayerID(), ePulse::ReportPlayer, std::chrono::minutes(1));
	}
}
// The player's auto-hunt (client-root/uiautohunt.py) asks which monster to go
// for. The client has no list of the characters round it - the scripts that
// do this without the server scan a million VIDs a frame - and the sectree
// has one. Monsters, and Metin stones when the window asks for them; only what
// battle_is_attackable lets this character hit; within the range of the point
// the hunt started from. What is already hitting the hunter comes first, then
// the nearest - except that a stone, when the window asks for stones, comes
// before every monster (playerbotify.py, apply_auto_hunt_stone_priority).
// A fifth argument names a VID the client gave up on as out of its reach.
// The answer is "AutoHuntTarget <vid>", zero for nothing.
struct FAutoHuntTarget
{
	LPCHARACTER	m_ch;
	int		m_iAnchorX;
	int		m_iAnchorY;
	int		m_iRange;
	bool		m_bStones;
	DWORD		m_dwSkipVID;
	bool		m_bMobs;
	bool		m_bBosses;
	bool		m_bBossPriority;
	LPCHARACTER	m_pkBest;
	int		m_iBestScore;
	// MT2009_PLUS_AUTOHUNT_CROWD_V1 (struct) (server-patches/enginefixes): the
	// nearest one plain - what the hunter boxed in by a pack asks for
	// (uiautohunt.py, COMBAT_STUCK_SECONDS): no ranking by who is hitting it,
	// by stone or by boss, which would send it after the one it could not
	// reach again.
	bool		m_bNearest;
	// MT2009_PLUS_AUTOHUNT_SKIPLIST_V1 (struct): the targets the client gave up on in the last minute
	// (upstream 2.0.76, uiautohunt.py: up to 32 VIDs, the ninth argument).
	std::set<DWORD>	m_setSkip;
	// MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (struct) (Autor: blaki): the client's order, the tenth
	// argument: 1 Boss>Metin>Mob, 2 Boss>Mob>Metin, 3 Metin>Boss>Mob, 4 Metin>Mob>Boss,
	// 5 Mob>Boss>Metin, 6 Mob>Metin>Boss; 0 (or none) the ranking below.
	int		m_iPriorityOrder = 0;

	FAutoHuntTarget(LPCHARACTER ch, int anchorX, int anchorY, int range, bool stones, DWORD skipVID,
			bool mobs = true, bool bosses = true, bool bossPriority = false)
		: m_ch(ch), m_iAnchorX(anchorX), m_iAnchorY(anchorY), m_iRange(range), m_bStones(stones),
		m_dwSkipVID(skipVID), m_bMobs(mobs), m_bBosses(bosses), m_bBossPriority(bossPriority),
		m_pkBest(NULL), m_iBestScore(0x7fffffff), m_bNearest(false)
	{
	}

	void operator () (LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER))
			return;

		LPCHARACTER victim = (LPCHARACTER) ent;
		if (victim == m_ch || victim->IsDead())
			return;
		if (m_dwSkipVID && (DWORD) victim->GetVID() == m_dwSkipVID)
			return;
		// MT2009_PLUS_AUTOHUNT_SKIPLIST_V1 (skip)
		if (!m_setSkip.empty() && m_setSkip.count((DWORD) victim->GetVID()))
			return;
		if (!victim->IsMonster() && !(m_bStones && victim->IsStone()))
			return;
		// Auto Lowy 2.0: a boss only with "Bossy", any other monster only
		// with "Moby" (the old window sends neither and gets both).
		const bool boss = victim->IsMonster() && victim->GetMobRank() >= MOB_RANK_BOSS;
		// MT2009_PLUS_AUTOHUNT_MINIBOSS_V1 (rank) (server-patches/enginefixes):
		// a miniboss - rank S_KNIGHT: Chuong, Lykos, Scrofa, Bera, Tigris, the
		// Bestial Archer and Specialist, the dungeons' elite monsters - is a
		// target with "Moby" and with "Bossy" alike (only "Moby" took it).
		const bool miniboss = victim->IsMonster() && victim->GetMobRank() == MOB_RANK_S_KNIGHT;
		if (victim->IsMonster() && (boss ? !m_bBosses : (miniboss ? !(m_bMobs || m_bBosses) : !m_bMobs)))
			return;
		if (DISTANCE_APPROX(victim->GetX() - m_iAnchorX, victim->GetY() - m_iAnchorY) > m_iRange)
			return;
		if (!battle_is_attackable(m_ch, victim))
			return;

		int score = DISTANCE_APPROX(victim->GetX() - m_ch->GetX(), victim->GetY() - m_ch->GetY());
		// MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (score) (Autor: blaki): the nearest within the first
		// category of the order that has a target. 0 boss (a miniboss too with "Bossy"),
		// 1 Metin, 2 any other monster; the six orders are every permutation.
		if (!m_bNearest && m_iPriorityOrder >= 1 && m_iPriorityOrder <= 6)
		{
			static const int s_aiOrder[6][3] = {
				{0, 1, 2}, {0, 2, 1}, {1, 0, 2},
				{1, 2, 0}, {2, 0, 1}, {2, 1, 0}
			};
			const int category = (boss || (miniboss && m_bBosses)) ? 0 : (victim->IsStone() ? 1 : 2);
			for (int rank = 0; rank < 3; ++rank)
				if (s_aiOrder[m_iPriorityOrder - 1][rank] == category)
				{
					score += rank * 1000000;
					break;
				}
			if (score < m_iBestScore)
			{
				m_iBestScore = score;
				m_pkBest = victim;
			}
			return;
		}
		// MT2009_PLUS_AUTOHUNT_CROWD_V1 (score): asked for the nearest, the
		// distance alone.
		if (m_bNearest)
		{
			if (score < m_iBestScore)
			{
				m_iBestScore = score;
				m_pkBest = victim;
			}
			return;
		}
		if (victim->GetVictim() == m_ch)
			score /= 4;
		// Asked for, a stone outranks every monster; the nearest stone wins.
		if (m_bStones && victim->IsStone())
			score -= 1000000;
		// And a boss before a stone, when the new window asks for bosses.
		if (m_bBossPriority && boss)
			score -= 2000000;
		// MT2009_PLUS_AUTOHUNT_MINIBOSS_V1 (order): with "Bossy", a miniboss
		// right after the boss and before the Metin.
		else if (m_bBossPriority && miniboss)
			score -= 1500000;
		if (score < m_iBestScore)
		{
			m_iBestScore = score;
			m_pkBest = victim;
		}
	}
};

// MT2009_PLUS_AUTOHUNT_MOUNT_V1 (command) (Autor: blaki): Auto Lowy gets off and back on a
// mount that holds the hunter in place (uiautohunt.py, MOUNT_STUCK_SECONDS). /ride is a
// toggle, and a retry after a late success would get the rider off again: "off" and "on"
// do something only when needed, and every call answers "AutoHuntMount <action> <mounted>".
// "on" brings back only the summoned horse or the worn mount seal, never a ride item from
// the bag.
ACMD(do_autohunt_mount)
{
	if (!ch || ch->IsDead() || !ch->GetSectree())
		return;
	char action[32];
	one_argument(argument, action, sizeof(action));
	if (strcmp(action, "off") && strcmp(action, "on"))
		return;
	const bool mounted = ch->IsHorseRiding() || ch->GetMountVnum() != 0;
	if (!strcmp(action, "off") && mounted)
		do_ride(ch, "", 0, 0);
	else if (!strcmp(action, "on") && !mounted)
	{
		bool available = ch->GetHorse() != NULL;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		available = available || ch->GetWear(WEAR_COSTUME_MOUNT) != NULL;
#endif
		if (available)
			do_ride(ch, "", 0, 0);
	}
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntMount %s %d", action,
			(ch->IsHorseRiding() || ch->GetMountVnum() != 0) ? 1 : 0);
}

// MT2009_PLUS_AUTOHUNT_BLOCKED_V1 (segment) (Autor: blaki): whether a wall, a rock or a
// building (ATTR_BLOCK | ATTR_OBJECT, the map's own 50-unit grid) stands on the straight
// line from the hunter to its target - the target stays selectable, the client asks
// /autohunt_path for the way round. The two ends are not tested: a monster may stand on
// the edge of a blocked cell.
static bool AutoHuntSegmentBlocked(long lMapIndex, long sx, long sy, long tx, long ty)
{
	const long dx = tx - sx;
	const long dy = ty - sy;
	const long length = DISTANCE_APPROX(dx, dy);
	if (length <= 100)
		return false;
	for (long step = 50; step <= length - 50; step += 50)
	{
		const long x = sx + dx * step / length;
		const long y = sy + dy * step / length;
		if (!SECTREE_MANAGER::instance().IsMovablePosition(lMapIndex, x, y))
			return true;
	}
	return false;
}

ACMD(do_autohunt_target)
{
	// playerbot: Auto Lowy switched off for this world (target).
	if (quest::CQuestManager::instance().GetEventFlag("m2_autohunt_off"))
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntOff");
		return;
	}
	// playerbot: Auto Lowy only with the ItemShop's time (target).
	if (quest::CQuestManager::instance().GetEventFlag("m2_autohunt_item"))
	{
		if (!ch->IsLoadedAffect())
			return;
		if (!ch->FindAffect(560))
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntOff item");
			return;
		}
	}
	char arg1[256], arg2[256], arg3[256], arg4[256], arg5[256];
	const char * rest = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	rest = two_arguments(rest, arg3, sizeof(arg3), arg4, sizeof(arg4));
	one_argument(rest, arg5, sizeof(arg5));

	if (!ch->GetSectree() || ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntTarget 0");
		return;
	}

	int range = 2000;
	int stones = 0;
	str_to_number(range, arg1);
	str_to_number(stones, arg2);
	range = MAX(300, MIN(range, 5000));

	int anchorX = ch->GetX();
	int anchorY = ch->GetY();
	if (*arg3 && *arg4)
	{
		int x = 0;
		int y = 0;
		str_to_number(x, arg3);
		str_to_number(y, arg4);
		// The client counts a position from its own map's corner and the
		// server from the world's (the client's network stream takes the
		// map's base off every position it receives), so the start point
		// comes as an offset from where the character stands. One further
		// than the sectrees round the character reach is a stale hunt from
		// another place: hunt round the character instead.
		if (DISTANCE_APPROX(x, y) <= 10000)
		{
			anchorX += x;
			anchorY += y;
		}
	}

	// The VID the client gave up on, if it names one (uiautohunt.py).
	DWORD skipVID = 0;
	if (*arg5)
		str_to_number(skipVID, arg5);

	// Auto Lowy 2.0 sends "<mobs> <bosses>" fifth and sixth and its skip
	// VID seventh; the old window names the VID fifth and nothing after
	// it (playerbotify apply_auto_hunt_categories).
	int mobs = 1;
	int bosses = 1;
	bool categories = false;
	// MT2009_PLUS_AUTOHUNT_CROWD_V1 (argument): an eighth argument of 1 after
	// the skip VID asks for the nearest monster plain - the hunter boxed in on
	// its way leaves the target it could not reach (the seventh) and takes one
	// of the pack round it. A client without it sends seven.
	int nearest = 0;
	char skipList[512] = ""; // MT2009_PLUS_AUTOHUNT_SKIPLIST_V1 (declare)
	char priorityArg[64] = ""; // MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (declare) (Autor: blaki)
	{
		char skip1[256], skip2[256], skip3[256], skip4[256], skip5[256], arg6[256], arg7[256], arg8[256];
		const char * more = two_arguments(argument, skip1, sizeof(skip1), skip2, sizeof(skip2));
		more = two_arguments(more, skip3, sizeof(skip3), skip4, sizeof(skip4));
		more = one_argument(more, skip5, sizeof(skip5));
		more = two_arguments(more, arg6, sizeof(arg6), arg7, sizeof(arg7));
		more = one_argument(more, arg8, sizeof(arg8));
		// MT2009_PLUS_AUTOHUNT_SKIPLIST_V1 (argument): "123,456,..." ninth.
		more = one_argument(more, skipList, sizeof(skipList));
		// MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (argument) (Autor: blaki): the order tenth; a client
		// that sends it sends the ninth too ("0" for no skips). Fewer arguments: as before.
		one_argument(more, priorityArg, sizeof(priorityArg));
		if (*arg6)
		{
			categories = true;
			str_to_number(mobs, arg5);
			str_to_number(bosses, arg6);
			skipVID = 0;
			if (*arg7)
				str_to_number(skipVID, arg7);
			if (*arg8)
				str_to_number(nearest, arg8);
		}
	}

	FAutoHuntTarget f(ch, anchorX, anchorY, range, stones != 0, skipVID);
	f.m_bMobs = mobs != 0;
	f.m_bBosses = bosses != 0;
	f.m_bBossPriority = categories && bosses != 0;
	f.m_bNearest = nearest == 1;
	// MT2009_PLUS_AUTOHUNT_PRIORITY_V1 (fill) (Autor: blaki): 0 the nearest plain, 1-6 an order.
	if (categories && *priorityArg)
	{
		int order = 0;
		str_to_number(order, priorityArg);
		order = MAX(0, MIN(order, 6));
		if (order == 0)
			f.m_bNearest = true;
		else
			f.m_iPriorityOrder = order;
	}
	// MT2009_PLUS_AUTOHUNT_SKIPLIST_V1 (fill)
	for (char* p = skipList; *p && f.m_setSkip.size() < 32; )
	{
		DWORD vid = (DWORD) strtoul(p, &p, 10);
		if (vid)
			f.m_setSkip.insert(vid);
		while (*p && (*p < '0' || *p > '9'))
			++p;
	}
	ch->GetSectree()->ForEachAround(f);
	// MT2009_PLUS_AUTOHUNT_BLOCKED_V1 (reply) (Autor: blaki): "AutoHuntTarget <vid> <blocked>";
	// a client before it reads the VID alone.
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntTarget %u %d",
			f.m_pkBest ? (unsigned int) (DWORD) f.m_pkBest->GetVID() : 0,
			(f.m_pkBest && AutoHuntSegmentBlocked(ch->GetMapIndex(), ch->GetX(), ch->GetY(),
					f.m_pkBest->GetX(), f.m_pkBest->GetY())) ? 1 : 0);
	// MT2009_PLUS_AUTOHUNT_PATH_V1 (hello): once a map, the ground's kind for /autohunt_path -
	// "AutoHuntPath 0 hello <0 none|1 grid|2 corridors>" (uiautohunt.py).
	{
		static std::map<DWORD, long> s_mapHelloMap;
		long& said = s_mapHelloMap[ch->GetPlayerID()];
		if (said != ch->GetMapIndex())
		{
			said = ch->GetMapIndex();
			ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntPath 0 hello %d",
					CPlayerBotManager::instance().GetHumanPathKind(ch->GetMapIndex()));
		}
	}
}

// MT2009_PLUS_AUTOHUNT_PATH_V1 (command): "/autohunt_path <seq> <vid> <dx> <dy>" - the way round
// the walls to a monster (its VID, read here) or to a point given as an offset
// from the character (the hunt's start), from the bots' route planner
// (CPlayerBotManager::PlanHumanPath). Answered "AutoHuntPath <seq> <ok|direct|
// none|wait|off> <kind> [<dx>,<dy>;...]", every point an offset from the
// character as the client counts them (upstream 2.0.76's protocol).
ACMD(do_autohunt_path)
{
	if (!ch || !ch->GetDesc() || !ch->GetSectree())
		return;
	char a1[64], a2[64], a3[64], a4[64];
	const char* rest = two_arguments(argument, a1, sizeof(a1), a2, sizeof(a2));
	two_arguments(rest, a3, sizeof(a3), a4, sizeof(a4));
	unsigned int seq = 0;
	DWORD vid = 0;
	long dx = 0, dy = 0;
	str_to_number(seq, a1);
	str_to_number(vid, a2);
	str_to_number(dx, a3);
	str_to_number(dy, a4);
	const int kind = CPlayerBotManager::instance().GetHumanPathKind(ch->GetMapIndex());
	if (kind == 0)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntPath %u off 0", seq);
		return;
	}
	long tx = ch->GetX() + dx, ty = ch->GetY() + dy;
	if (vid)
	{
		LPCHARACTER target = CHARACTER_MANAGER::instance().Find(vid);
		if (target && target->GetMapIndex() == ch->GetMapIndex())
		{
			tx = target->GetX();
			ty = target->GetY();
		}
	}
	std::vector<std::pair<long, long> > points;
	const int r = CPlayerBotManager::instance().PlanHumanPath(ch, tx, ty, points);
	if (r == -1)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntPath %u wait %d", seq, kind);
		return;
	}
	if (r <= 0 || points.empty())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntPath %u none %d", seq, kind);
		return;
	}
	if (points.size() <= 1)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntPath %u direct %d", seq, kind);
		return;
	}
	// The chat line holds about 450 characters: at most 24 points, spread evenly
	// over the route, the target last.
	std::string line;
	const size_t keep = std::min<size_t>(points.size(), 24);
	for (size_t i = 0; i < keep; ++i)
	{
		const size_t k = keep <= 1 ? points.size() - 1 : (i * (points.size() - 1)) / (keep - 1);
		char one[32];
		snprintf(one, sizeof(one), "%s%ld,%ld", line.empty() ? "" : ";",
				points[k].first - ch->GetX(), points[k].second - ch->GetY());
		line += one;
	}
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntPath %u ok %d %s", seq, kind, line.c_str());
}

// The auto-hunt's pick-up by kind (client-root/uiautohunt.py). The client's
// own PickCloseItem takes whatever lies nearest and cannot tell a sword from a
// potion, and the window offers "do not pick up weapons, armour, ..." (Tieru,
// 15 September). So the client asks "/autohunt_loot <range> <kinds> <x> <y>"
// and is answered "AutoHuntLoot <vid> <x> <y>": the nearest item on the ground
// this character may take, of a kind the window keeps, within the range of
// the point the hunt started from - zero for nothing. The client walks there
// and sends the ordinary pick-up packet, which CHARACTER::PickupItem judges as
// it judges anybody's. Yang goes with every kind.
enum
{
	AUTOHUNT_LOOT_WEAPON = 1 << 0,
	AUTOHUNT_LOOT_ARMOUR = 1 << 1,
	AUTOHUNT_LOOT_JEWELLERY = 1 << 2,
	AUTOHUNT_LOOT_POTION = 1 << 3,
	AUTOHUNT_LOOT_BOOK = 1 << 4,
	AUTOHUNT_LOOT_STONE = 1 << 5,
	AUTOHUNT_LOOT_OTHER = 1 << 6,
	// Client 2.0.41 split these off the armour and the jewellery
	// ("nie podnos kolczykow, butow, naszyjnikow").
	AUTOHUNT_LOOT_HELMET = 1 << 7,
	AUTOHUNT_LOOT_SHIELD = 1 << 8,
	AUTOHUNT_LOOT_BRACELET = 1 << 9,
	AUTOHUNT_LOOT_SHOES = 1 << 10,
	AUTOHUNT_LOOT_NECKLACE = 1 << 11,
	AUTOHUNT_LOOT_EARRINGS = 1 << 12,
};

static int AutoHuntLootKind(LPITEM item)
{
	switch (item->GetType())
	{
		case ITEM_WEAPON:
			return item->GetSubType() == WEAPON_ARROW ? AUTOHUNT_LOOT_OTHER : AUTOHUNT_LOOT_WEAPON;
		case ITEM_ARMOR:
			switch (item->GetSubType())
			{
				case ARMOR_BODY:
					return AUTOHUNT_LOOT_ARMOUR;
				case ARMOR_HEAD:
					return AUTOHUNT_LOOT_HELMET;
				case ARMOR_SHIELD:
					return AUTOHUNT_LOOT_SHIELD;
				case ARMOR_WRIST:
					return AUTOHUNT_LOOT_BRACELET;
				case ARMOR_FOOTS:
					return AUTOHUNT_LOOT_SHOES;
				case ARMOR_NECK:
					return AUTOHUNT_LOOT_NECKLACE;
				case ARMOR_EAR:
					return AUTOHUNT_LOOT_EARRINGS;
				default:
					return AUTOHUNT_LOOT_JEWELLERY;
			}
		case ITEM_RING:
		case ITEM_BELT:
			return AUTOHUNT_LOOT_JEWELLERY;
		case ITEM_USE:
			switch (item->GetSubType())
			{
				case USE_POTION:
				case USE_POTION_NODELAY:
				case USE_ABILITY_UP:
					return AUTOHUNT_LOOT_POTION;
				default:
					return AUTOHUNT_LOOT_OTHER;
			}
		case ITEM_SKILLBOOK:
		case ITEM_SKILLFORGET:
			return AUTOHUNT_LOOT_BOOK;
		case ITEM_METIN:
			return AUTOHUNT_LOOT_STONE;
		default:
			return AUTOHUNT_LOOT_OTHER;
	}
}

struct FAutoHuntLoot
{
	LPCHARACTER m_ch;
	int m_iAnchorX;
	int m_iAnchorY;
	int m_iRange;
	int m_iKinds;
	LPITEM m_pkBest;
	int m_iBestDistance;

	FAutoHuntLoot(LPCHARACTER ch, int anchorX, int anchorY, int range, int kinds)
		: m_ch(ch), m_iAnchorX(anchorX), m_iAnchorY(anchorY), m_iRange(range), m_iKinds(kinds),
		m_pkBest(NULL), m_iBestDistance(0x7fffffff)
	{
	}

	void operator () (LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_ITEM))
			return;

		LPITEM item = (LPITEM) ent;
		if (item->GetOwner() || !item->GetSectree())
			return;
		if (item->GetType() != ITEM_ELK && !(AutoHuntLootKind(item) & m_iKinds))
			return;
		// MT2009_PLUS_PICKUP_FILTER_V1 (auto hunt): one pick-up filter for the Z key,
		// Auto Lowy and the companion (the operator, 30 September: "filtr dzialal
		// na wszystko: czyli na nas, na autolowy i na towarzysza jednoczesnie").
		// The client asks for the filter's kinds already (uiautohunt.LootMask);
		// the filter the client last sent (/pickup_filter, char_item.cpp) holds
		// here too, for a client that asks for more. Yang always passes.
		{
			bool Mt2009PlusPickupFilterAllows(LPCHARACTER ch, LPITEM item);
			if (!Mt2009PlusPickupFilterAllows(m_ch, item))
				return;
		}
		if (DISTANCE_APPROX(item->GetX() - m_iAnchorX, item->GetY() - m_iAnchorY) > m_iRange)
			return;
		if (!item->IsOwnership(m_ch))
			return;

		const int distance = DISTANCE_APPROX(item->GetX() - m_ch->GetX(), item->GetY() - m_ch->GetY());
		if (distance < m_iBestDistance)
		{
			m_iBestDistance = distance;
			m_pkBest = item;
		}
	}
};

ACMD(do_autohunt_loot)
{
	// playerbot: Auto Lowy switched off for this world (loot).
	if (quest::CQuestManager::instance().GetEventFlag("m2_autohunt_off"))
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntOff");
		return;
	}
	// playerbot: Auto Lowy only with the ItemShop's time (loot).
	if (quest::CQuestManager::instance().GetEventFlag("m2_autohunt_item"))
	{
		if (!ch->IsLoadedAffect())
			return;
		if (!ch->FindAffect(560))
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntOff item");
			return;
		}
	}
	char arg1[256], arg2[256], arg3[256], arg4[256], arg5[256];
	const char * rest = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	rest = two_arguments(rest, arg3, sizeof(arg3), arg4, sizeof(arg4));
	one_argument(rest, arg5, sizeof(arg5));

	int range = 2000;
	int kinds = 0;
	str_to_number(range, arg1);
	str_to_number(kinds, arg2);
	// Client 2.0.41 sends every kind it keeps as a fifth field and the
	// seven kinds of 2.0.40 as the second, for a server before this one.
	// Without the fifth - an older client - "armour" still means the
	// helmet and the shield too, and "jewellery" every worn trinket.
	if (*arg5)
		str_to_number(kinds, arg5);
	else
	{
		if (kinds & AUTOHUNT_LOOT_ARMOUR)
			kinds |= AUTOHUNT_LOOT_HELMET | AUTOHUNT_LOOT_SHIELD;
		if (kinds & AUTOHUNT_LOOT_JEWELLERY)
			kinds |= AUTOHUNT_LOOT_BRACELET | AUTOHUNT_LOOT_SHOES | AUTOHUNT_LOOT_NECKLACE | AUTOHUNT_LOOT_EARRINGS;
	}
	range = MAX(300, MIN(range, 5000));

	if (!ch->GetSectree() || ch->IsDead() || kinds <= 0)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntLoot 0 0 0");
		return;
	}

	int anchorX = ch->GetX();
	int anchorY = ch->GetY();
	if (*arg3 && *arg4)
	{
		int x = 0;
		int y = 0;
		str_to_number(x, arg3);
		str_to_number(y, arg4);
		// The same rule as the target: an offset from the character, and a
		// stale one hunts round the character.
		if (DISTANCE_APPROX(x, y) <= 10000)
		{
			anchorX += x;
			anchorY += y;
		}
	}

	FAutoHuntLoot f(ch, anchorX, anchorY, range, kinds);
	ch->GetSectree()->ForEachAround(f);
	if (!f.m_pkBest)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntLoot 0 0 0");
		return;
	}
	// The item's place as an offset from the character, which the client adds
	// to its own position: in the world's coordinates every item stood a
	// map's base away from a client that counts from its map's corner, and
	// the pick-up never came within reach (Tieru, 15 September).
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntLoot %u %ld %ld",
			(unsigned int) (DWORD) f.m_pkBest->GetVID(),
			(long) (f.m_pkBest->GetX() - ch->GetX()), (long) (f.m_pkBest->GetY() - ch->GetY()));
}

// "Scal i uporzadkuj" - the inventory's button (client-root/inventoryarrange.py).
// One request instead of a move for every pair of stacks: the old button sent
// three hundred in a frame and the flood limit closed the connection, and a
// queue of moves could only pour stacks, never lay the pages out. The server
// does both at once (playerbot_arrange.cpp) and answers with what it did.
// No option is known yet, and one that is not is refused rather than guessed.
ACMD(do_inventory_arrange)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	playerbot_arrange::TResult result;
	// MT2009_PLUS_ARRANGE_MERGE_V1 (server-patches/playerqol): "merge" pours
	// the stacks together and moves nothing else ("samo laczenie w stacki bez
	// sortowania", the operator, 28 September).
	// MT2009_PLUS_INVENTORY_SORT_LOCK_V1 (server-patches/sortlock): the words
	// - "merge" and the cells the player locked with Alt + left click,
	// "keep=<hex>" - are read by playerbot_arrange::InventoryArrangeCommand,
	// which leaves the locked items where they stand and lays the rest of
	// the bag out round them; an unknown word is still RESULT_BAD_REQUEST.
	result = playerbot_arrange::InventoryArrangeCommand(ch, argument);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "InventoryArrangeResult %d %d %d %u",
			result.code, result.moved, result.merged, result.units);
}

// "Podnies caly drop" - the ` key (client-root/pickupnearby.py); the work is
// CHARACTER::PickupNearbyItems in char_item.cpp.
ACMD(do_pickup_nearby)
{
	ch->PickupNearbyItems();
}

// MT2009_PLUS_PICKUP_FILTER_V1 (commands): "/pickup_filter <on> <kinds>" from
// the client's filter window, and "/filtr", which opens it.
extern std::map<DWORD, DWORD> g_mapMt2009PlusPickupFilter;

// MT2009_PLUS_PICKUP_BONUS_FILTER_V1 (command): "/pickup_filter <on> <kinds>
// [<bonus kinds> <min bonuses>]" - the kinds picked up only with bonuses, and
// how many (1-5, char_item.cpp). A client that sends two numbers has no
// "Bonus" kind; the answer carries both numbers too, which a client that
// reads two ignores.
DWORD Mt2009PlusSetPickupBonus(DWORD pid, DWORD kinds, int& minCount);

ACMD(do_pickup_filter)
{
	char arg1[32], arg2[32], arg3[32], arg4[32];
	const char* rest = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	two_arguments(rest, arg3, sizeof(arg3), arg4, sizeof(arg4));
	int on = 0;
	unsigned long kinds = 0;
	unsigned long bonusKinds = 0;
	int bonusMin = 1;
	str_to_number(on, arg1);
	str_to_number(kinds, arg2);
	if (*arg3)
		str_to_number(bonusKinds, arg3);
	if (*arg4)
		str_to_number(bonusMin, arg4);
	if (on)
		g_mapMt2009PlusPickupFilter[ch->GetPlayerID()] = (DWORD) (kinds & 0x1FFF);
	else
		g_mapMt2009PlusPickupFilter.erase(ch->GetPlayerID());
	const DWORD bonus = Mt2009PlusSetPickupBonus(ch->GetPlayerID(), on ? (DWORD) (bonusKinds & kinds) : 0, bonusMin);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "PickupFilterAck %d %lu %u %d", on ? 1 : 0, kinds & 0x1FFF, (unsigned int) bonus, bonusMin);
}

// MT2009_PLUS_COSTUME_HIDE_V1 (command): "/kostiumy_ukryj <0|1>" from the
// costume window (item.cpp, Mt2009PlusApplyCostumeParts).
extern std::map<DWORD, bool> g_mapMt2009PlusCostumeHidden;
void Mt2009PlusApplyCostumeParts(LPCHARACTER ch, LPITEM leaving);

ACMD(do_costume_hide)
{
	char arg1[32];
	one_argument(argument, arg1, sizeof(arg1));
	int hide = 0;
	str_to_number(hide, arg1);
	if (hide)
		g_mapMt2009PlusCostumeHidden[ch->GetPlayerID()] = true;
	else
		g_mapMt2009PlusCostumeHidden.erase(ch->GetPlayerID());
	Mt2009PlusApplyCostumeParts(ch, NULL);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "CostumeHiddenAck %d", hide ? 1 : 0);
}

ACMD(do_pickup_filter_open)
{
	ch->ChatPacket(CHAT_TYPE_COMMAND, "OpenPickupFilter");
}

// MT2009_PLUS_GARBAGE_BATCH_V1 (server-patches/playerqol): the bin's stacks
// many to a command. "/garbage prepare_many <req> <slot>:<vnum>:<count> ..."
// proves every stack exactly as prepare does and keeps the tokens as one
// batch, answered "GarbageBinPreparedMany <req> <batch> <n>" or
// "GarbageBinRejectedMany <req> <index> <reason>"; "/garbage commit_many <req>
// <batch>" destroys them as commit does, answered "GarbageBinResultMany <req>
// OK <n>" or "GarbageBinResultMany <req> <reason> <done>". One stack at a
// time was two round trips and a pause each: half a minute for a full bin
// ("sprawdzanie i usuwanie trwa wieki", the operator, 28 September).
static std::map<DWORD, std::pair<std::string, std::vector<std::string> > > s_mapMt2009PlusGarbageBatches;

static void Mt2009PlusGarbageBinBatch(LPCHARACTER ch, bool prepare, const char* args)
{
	char reqid[16] = "";
	const char* rest = one_argument(args, reqid, sizeof(reqid));
	if (!reqid[0])
		strlcpy(reqid, "0", sizeof(reqid));
	if (ch->GarbageBinCheckRateLimit())
	{
		if (prepare)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejectedMany %s -1 RATE_LIMIT", reqid);
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResultMany %s RATE_LIMIT 0", reqid);
		return;
	}
	const DWORD pid = ch->GetPlayerID();
	if (prepare)
	{
		std::vector<std::string> tokens;
		char entry[64];
		int index = 0;
		while (true)
		{
			rest = one_argument(rest, entry, sizeof(entry));
			if (!entry[0])
				break;
			unsigned long slot = 0, vnum = 0, count = 0;
			if (sscanf(entry, "%lu:%lu:%lu", &slot, &vnum, &count) != 3 || slot > 65535 || vnum == 0 ||
					vnum > 2147483647 || count == 0 || count > 2147483647 || tokens.size() >= 36)
			{
				ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejectedMany %s %d INVALID", reqid, index);
				return;
			}
			char token[33];
			const char* reason = ch->GarbageBinPrepare(TItemPos(INVENTORY, (WORD) slot), (DWORD) vnum,
					(ITEM_COUNT) count, token);
			if (reason)
			{
				ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejectedMany %s %d %s", reqid, index, reason);
				return;
			}
			tokens.push_back(token);
			++index;
		}
		if (tokens.empty())
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejectedMany %s 0 INVALID", reqid);
			return;
		}
		s_mapMt2009PlusGarbageBatches[pid] = std::make_pair(tokens[0], tokens);
		ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinPreparedMany %s %s %d", reqid, tokens[0].c_str(), (int) tokens.size());
		return;
	}

	char batch[64] = "";
	one_argument(rest, batch, sizeof(batch));
	std::map<DWORD, std::pair<std::string, std::vector<std::string> > >::iterator it =
			s_mapMt2009PlusGarbageBatches.find(pid);
	if (!batch[0] || it == s_mapMt2009PlusGarbageBatches.end() || it->second.first != batch)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResultMany %s NOT_FOUND 0", reqid);
		return;
	}
	const std::vector<std::string> tokens = it->second.second;
	s_mapMt2009PlusGarbageBatches.erase(it);
	int done = 0;
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		const char* reason = ch->GarbageBinCommit(tokens[i].c_str());
		if (reason)
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResultMany %s %s %d", reqid, reason, done);
			return;
		}
		++done;
	}
	ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResultMany %s OK %d", reqid, done);
}

// "/garbage" - the Garbage Bin's chat-command protocol v1 (see
// custom-patches/garbage_bin.patch for the full contract). The client sends
// "/garbage hello|prepare|commit ...", we answer through CHAT_TYPE_COMMAND
// exactly like RefineSuceeded/OpenPrivateShop/StoneDetect already do, so no
// new packet header or client EXE change is needed.
//
// hello never touches the rate limiter: it is a pure capability probe, not
// an action, and gating it would make the client's own "is the feature even
// there" check subject to the same 250ms pacing as a real prepare/commit.
// MT2009_PLUS_AUTO_TARGET_V2 (command): the client's next target after a
// kill (root/autotarget.py) may be a monster attacking the player that the
// player has not struck yet - which only the server knows. "/autotarget_aggro
// 0" is the probe, answered "AutoTargetAggroReady 1"; "/autotarget_aggro <req>"
// (req > 0, the client's tag of one kill) is answered "AutoTargetAggro <req>
// <vid,vid,...>" (or "0") with the living monsters near the player whose
// victim is the player, nearest first, at most MT2009_PLUS_AGGRO_MAX. At most
// one answer a MT2009_PLUS_AGGRO_GAP_MS a player; the client asks once a kill.
static const DWORD MT2009_PLUS_AGGRO_GAP_MS = 300;
static const size_t MT2009_PLUS_AGGRO_MAX = 16;
static const int MT2009_PLUS_AGGRO_RANGE = 2000;
static std::map<DWORD, DWORD> s_mapMt2009PlusAggroAskedAt;

struct FMt2009PlusFindAggressors
{
	LPCHARACTER m_me;
	std::vector<std::pair<int, DWORD> > m_found;

	FMt2009PlusFindAggressors(LPCHARACTER me) : m_me(me) {}

	void operator () (LPENTITY ent)
	{
		if (!ent || !ent->IsType(ENTITY_CHARACTER))
			return;
		LPCHARACTER mob = (LPCHARACTER) ent;
		if (mob == m_me || !mob->IsMonster() || mob->IsDead() || mob->m_kVIDVictim != m_me->GetVID())
			return;
		const int distance = DISTANCE_APPROX(mob->GetX() - m_me->GetX(), mob->GetY() - m_me->GetY());
		if (distance > MT2009_PLUS_AGGRO_RANGE)
			return;
		m_found.push_back(std::make_pair(distance, (DWORD) mob->GetVID()));
	}
};

ACMD(do_autotarget_aggro)
{
	if (!ch || !ch->GetDesc() || !ch->GetSectree())
		return;

	char arg1[32];
	one_argument(argument, arg1, sizeof(arg1));
	long request = 0;
	if (*arg1)
		request = strtol(arg1, NULL, 10);
	if (request <= 0)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoTargetAggroReady 1");
		return;
	}

	const DWORD now = get_dword_time();
	std::map<DWORD, DWORD>::iterator asked = s_mapMt2009PlusAggroAskedAt.find(ch->GetPlayerID());
	if (asked != s_mapMt2009PlusAggroAskedAt.end() && now - asked->second < MT2009_PLUS_AGGRO_GAP_MS)
		return;
	s_mapMt2009PlusAggroAskedAt[ch->GetPlayerID()] = now;
	if (s_mapMt2009PlusAggroAskedAt.size() > 4096)
		s_mapMt2009PlusAggroAskedAt.clear();

	FMt2009PlusFindAggressors f(ch);
	ch->GetSectree()->ForEachAround(f);
	std::sort(f.m_found.begin(), f.m_found.end());

	std::string vids;
	for (size_t i = 0; i < f.m_found.size() && i < MT2009_PLUS_AGGRO_MAX; ++i)
	{
		if (!vids.empty())
			vids += ",";
		vids += std::to_string(f.m_found[i].second);
	}
	if (vids.empty())
		vids = "0";
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoTargetAggro %ld %s", request, vids.c_str());
}

ACMD(do_garbage)
{
	if (!ch)
		return;

	char garbage_verb[16] = "";
	const char * rest = one_argument(argument, garbage_verb, sizeof(garbage_verb));

	if (!garbage_verb[0])
		return;

	if (!strcmp(garbage_verb, "hello"))
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinReady 1");
		// MT2009_PLUS_GARBAGE_BATCH_V1 (hello): a client that knows the batch
		// uses it; one that does not ignores the line.
		ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinBatch 1");
		return;
	}

	if (!strcmp(garbage_verb, "prepare_many") || !strcmp(garbage_verb, "commit_many"))
	{
		Mt2009PlusGarbageBinBatch(ch, !strcmp(garbage_verb, "prepare_many"), rest);
		return;
	}

	if (strcmp(garbage_verb, "prepare") && strcmp(garbage_verb, "commit"))
		return;

	// The request_id is only for the client to match a reply to its own
	// request; grab it now, before the rate-limit check, so a RATE_LIMIT
	// refusal can still name the request it refused instead of always
	// answering "0". `afterReqid` is what's left of the argument string
	// after it -- window_type/slot/vnum/count for prepare, the token alone
	// for commit.
	char reqid[16] = "";
	const char * afterReqid = one_argument(rest, reqid, sizeof(reqid));
	if (!reqid[0])
		strlcpy(reqid, "0", sizeof(reqid));

	if (ch->GarbageBinCheckRateLimit())
	{
		if (!strcmp(garbage_verb, "prepare"))
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejected %s RATE_LIMIT", reqid);
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResult %s RATE_LIMIT", reqid);
		return;
	}

	if (!strcmp(garbage_verb, "prepare"))
	{
		unsigned long window_type = 0, slot = 0, vnum = 0, count = 0;
		if (sscanf(afterReqid, "%lu %lu %lu %lu", &window_type, &slot, &vnum, &count) != 4)
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejected %s INVALID", reqid);
			return;
		}

		// The client sends its own player.INVENTORY constant; only that
		// exact value (never a hardcoded 1) and only a cell number that
		// fits in the packed position's WORD are acceptable.
		if (window_type != (unsigned long) INVENTORY || slot > 65535 || vnum == 0 || vnum > 2147483647 ||
			count == 0 || count > 2147483647)
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejected %s FORBIDDEN", reqid);
			return;
		}

		TItemPos pos(INVENTORY, (WORD) slot);
		char token[33];
		const char * reason = ch->GarbageBinPrepare(pos, (DWORD) vnum, (ITEM_COUNT) count, token);
		if (reason)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinRejected %s %s", reqid, reason);
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinPrepared %s %s", reqid, token);
		return;
	}

	// commit
	{
		char token[64] = "";
		one_argument(afterReqid, token, sizeof(token));

		if (!token[0])
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResult %s INVALID", reqid);
			return;
		}

		const char * reason = ch->GarbageBinCommit(token);
		if (reason)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResult %s %s", reqid, reason);
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "GarbageBinResult %s OK", reqid);
	}
}

// "/opengarbagebin" - tells this player's own client to open the Garbage Bin
// window. The client's existing command interpreter already recognizes
// "OpenGarbageBin" (and, per the protocol, "GarbageBin") through
// CHAT_TYPE_COMMAND; this is the server-side trigger the protocol's own
// "opcjonalne otwarcie okna przez serwer" line calls for.
ACMD(do_open_garbage_bin)
{
	if (!ch)
		return;

	ch->ChatPacket(CHAT_TYPE_COMMAND, "OpenGarbageBin");
}

// "/mob_drop" - read-only preview of a selected monster or Metin stone's
// possible drop, protocol v1 (custom-patches/mob_drop_preview.patch). All
// enumeration happens in ITEM_MANAGER::GetPossibleMobDropItems(), a
// side-effect-free twin of CreateDropItem()/CreateQuestDropItem() -- see its
// own comment for exactly which sources it reads and why out_complete can
// come back false. This command only validates the target/session and
// paginates what that function returns.
ACMD(do_mob_drop)
{
	if (!ch)
		return;

	unsigned long request_id = 0, target_vid = 0, page = 0;
	int nParsed = sscanf(argument, "%lu %lu %lu", &request_id, &target_vid, &page);

	// request_id is an opaque echo, never authorization -- but it still has
	// to be a positive number the client can read back, and an unparsed
	// request has no request_id worth echoing at all.
	if (nParsed != 3 || request_id == 0 || request_id > 2147483647 || page > 1000000)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropError 0 INVALID_TARGET");
		return;
	}

	if (ch->MobDropCheckRateLimit())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropError %lu RATE_LIMIT", request_id);
		return;
	}

	// "z aktywnej sesji gry": no desc means no client on the other end
	// (already logging out, or a bot with no connection), and a dead
	// character has nothing to preview a drop for either.
	if (!ch->GetDesc() || ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropError %lu UNAVAILABLE", request_id);
		return;
	}

	LPCHARACTER pkTarget = CHARACTER_MANAGER::instance().Find((DWORD) target_vid);

	// Players, pets and ordinary NPCs are all IsMonster()==false,
	// IsStone()==false -- this one check is what "odrzucaj graczy, pety,
	// zwykle NPC" comes down to. Buildings are never CHARACTER instances
	// (see CInputMain::Target()'s own building::CManager lookup before its
	// CHARACTER_MANAGER one), so Find() already returns NULL for those.
	if (!pkTarget || !(pkTarget->IsMonster() || pkTarget->IsStone()) || pkTarget->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropError %lu INVALID_TARGET", request_id);
		return;
	}

	if (ch->GetMapIndex() != pkTarget->GetMapIndex())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropError %lu INVALID_TARGET", request_id);
		return;
	}

	// VIEW_RANGE is this server's own configured visibility distance
	// (config.cpp, CONFIG's VIEW_RANGE) -- the same one entity_view.cpp uses
	// to decide what a character can see at all, so a target further than
	// this could not have been legitimately clicked in the first place.
	if (DISTANCE_APPROX(ch->GetX() - pkTarget->GetX(), ch->GetY() - pkTarget->GetY()) > VIEW_RANGE)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropError %lu TOO_FAR", request_id);
		return;
	}

	std::set<DWORD> setVnums;
	bool bComplete = true;
	ITEM_MANAGER::instance().GetPossibleMobDropItems(ch, pkTarget, setVnums, bComplete);

	// std::set already iterates in ascending key order -- ITEM_MANAGER's own
	// dedup (it is a set) plus this is the whole "scal, odfiltruj, posortuj"
	// step; nothing left to do before paging.
	std::vector<DWORD> vnums(setVnums.begin(), setVnums.end());

	DWORD total = (DWORD) vnums.size();
	DWORD pages = MAX((DWORD) 1, (total + 7) / 8);

	if (page >= pages)
	{
		// The target and the request are both fine; only this page number
		// isn't one that exists. UNAVAILABLE reads truer than INVALID_TARGET
		// for that -- noted in the report as the chosen reading of the two
		// the protocol note allows here.
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropError %lu UNAVAILABLE", request_id);
		return;
	}

	DWORD pageStart = page * 8;
	DWORD pageCount = MIN((DWORD) 8, total - pageStart);

	ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropBegin %lu %lu %u %lu %lu %lu %d",
		request_id, target_vid, pkTarget->GetRaceNum(), page, pages, total, bComplete ? 1 : 0);

	for (DWORD i = 0; i < pageCount; ++i)
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropItem %lu %lu %u", request_id, i, vnums[pageStart + i]);

	ch->ChatPacket(CHAT_TYPE_COMMAND, "MobDropEnd %lu", request_id);
}

// The safebox's "Scal i uporzadkuj", and a stack moved by count across the
// safebox and the bag or inside the safebox (blasty's proposal, 19 September;
// client-root/safeboxtransfer.py). The safebox's packets name cells and no
// count, so a part of a stack, and a stack dropped on the same item, go by
// command; a whole stack dropped on a free place still goes by the packet.
// playerbot_arrange.cpp does the work, and every answer goes back for the
// client to put the refusals into words.
ACMD(do_safebox_arrange)
{
	char arg1[256];
	one_argument(argument, arg1, sizeof(arg1));
	playerbot_arrange::TResult result;
	// MT2009_PLUS_SAFEBOX_MERGE_V1 (server-patches/safeboxmerge): "merge"
	// pours the safebox's stacks together and moves nothing else - the
	// safebox's "Tylko scal stosy" button, as "/inventory_arrange merge" is
	// the bag's (playerbot_arrange::MergeSafeboxStacks).
	if (!strcmp(arg1, "merge"))
		result = playerbot_arrange::MergeSafeboxStacks(ch, true);
	else if (*arg1)
		result.code = playerbot_arrange::RESULT_BAD_REQUEST;
	else
		result = playerbot_arrange::ArrangeSafebox(ch, true);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "SafeboxArrangeResult %d %d %d %u",
			result.code, result.moved, result.merged, result.units);
}

// /safebox_put <bag cell> <safebox cell> [count], /safebox_take <safebox cell>
// <bag cell> [count], /safebox_move <safebox cell> <safebox cell> [count]; the
// subcommand is playerbot_arrange::ETransferOp, and no count, or 0, is the
// whole stack.
ACMD(do_safebox_transfer)
{
	char arg1[256], arg2[256], arg3[256];
	const char * rest = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	one_argument(rest, arg3, sizeof(arg3));
	unsigned int from = 0, to = 0, count = 0;
	playerbot_arrange::TTransfer result;
	result.code = playerbot_arrange::TRANSFER_BAD_REQUEST;
	if (*arg1 && *arg2 && str_to_number(from, arg1) && str_to_number(to, arg2) &&
			(!*arg3 || str_to_number(count, arg3)))
	{
		switch (subcmd)
		{
			case playerbot_arrange::TRANSFER_OP_PUT:
				result = playerbot_arrange::PutIntoSafebox(ch, from, to, count);
				break;
			case playerbot_arrange::TRANSFER_OP_TAKE:
				result = playerbot_arrange::TakeFromSafebox(ch, from, to, count);
				break;
			case playerbot_arrange::TRANSFER_OP_MOVE:
				result = playerbot_arrange::MoveInSafebox(ch, from, to, count);
				break;
		}
	}
	ch->ChatPacket(CHAT_TYPE_COMMAND, "SafeboxTransferResult %d %d %u", subcmd, result.code, result.units);
}

// "Towarzysz", the player's own companion (playerbot_sidekick.h): the
// Towarzysz quest's letter sends these - stworz, przywolaj, wolny, stan,
// odprawa - and a player may type them too. The manager answers in the chat.
ACMD(do_towarzysz)
{
	if (!ch || !ch->GetDesc())
		return;
	CPlayerBotManager::instance().OnSidekickCommand(ch, argument);
}

// MT2009_PLUS_DUNGEON_PANEL_V1 (command): the dungeon panel,
// "/lochy [open|warp <i>|rank <i> <type>|reload]" (playerbot_dungeon_panel.h).
ACMD(do_dungeon_panel)
{
	if (!ch || !ch->GetDesc())
		return;
	void DungeonPanelCommand(LPCHARACTER ch, const char* argument);
	DungeonPanelCommand(ch, argument);
}

// MT2009_PLUS_EVENT_MANAGER_V1 (command): the in-game event manager's
// "/ingame_event hello <caps>|info|gm" (playerbot_ingame_events.h).
ACMD(do_ingame_event)
{
	if (!ch || !ch->GetDesc())
		return;
	void InGameEventCommand(LPCHARACTER ch, const char* argument);
	InGameEventCommand(ch, argument);
}

// MT2009_PLUS_SEONHAE_V1 (command): Seon-Hae's 6th/7th bonus window
// "/seonhae open|add|collect|close|gm" (playerbot_seonhae.h).
ACMD(do_seonhae)
{
	if (!ch || !ch->GetDesc())
		return;
	void SeonHaeCommand(LPCHARACTER ch, const char* argument);
	SeonHaeCommand(ch, argument);
}

// MT2009_PLUS_GOBLIN_V1 (command): the Treasure Hunt's window and island,
// "/goblin [info|odkryj|szukaj|reset|tury|tura|ranking|wyjdz]" (playerbot_goblin.h).
ACMD(do_goblin)
{
	if (!ch || !ch->GetDesc())
		return;
	void GoblinCommand(LPCHARACTER ch, const char* argument);
	GoblinCommand(ch, argument);
}

// MT2009_PLUS_WHEEL_V1 (command): the Kolo Fortuny window's
// "/kolo [krec | odbierz <id>]" (playerbot_wheel.h).
ACMD(do_wheel)
{
	if (!ch || !ch->GetDesc())
		return;
	void WheelCommand(LPCHARACTER ch, const char* argument);
	WheelCommand(ch, argument);
}

// MT2009_PLUS_BATTLE_PASS_V1 (command): the Battle Pass window's
// "/battlepass [odbierz <id> | nagroda]" (playerbot_battlepass.h).
ACMD(do_battlepass)
{
	if (!ch || !ch->GetDesc())
		return;
	void BattlePassCommand(LPCHARACTER ch, const char* argument);
	BattlePassCommand(ch, argument);
}

// MT2009_PLUS_GUILD_DUTY_V1 (command): the guild leader's panel
// "/gildia_obowiazki [zrzutka <yang> <h> | zrzutka_anuluj | misja <vnum> <n> |
// misja_anuluj | wyplac <vnum> <n> | dt [n] | dt_anuluj]" (playerbot_guildduty.h).
ACMD(do_guild_duty)
{
	if (!ch || !ch->GetDesc())
		return;
	void GuildDutyCommand(LPCHARACTER ch, const char* argument);
	GuildDutyCommand(ch, argument);
}

// MT2009_PLUS_EVENT_CALENDAR_V1 (command): "/kalendarz" from the event
// calendar (client uieventcalendar.py, F11) - the panels' schedule as this
// core read it (playerbot_manager.cpp, SendEventCalendar). Once a second.
ACMD(do_event_calendar)
{
	if (!ch || !ch->GetDesc())
		return;
	static std::map<DWORD, DWORD> s_mapCalendarAskedAt;
	DWORD& asked = s_mapCalendarAskedAt[ch->GetPlayerID()];
	const DWORD now = get_dword_time();
	if (asked != 0 && now - asked < 1000)
		return;
	asked = now;
	CPlayerBotManager::instance().SendEventCalendar(ch);
}

// What a chest holds and what a monster can drop (Gibon's two windows, 26
// September; client-root/uichestpreview.py and uimobpreview.py). Both answer
// from what this process loaded - special_item_group.txt with its nested
// groups followed, and the monster's drop tables - so a window shows what the
// running server hands out, an operator's edited group included, and nothing
// about a chest or a drop is kept in the client.
static const int PLAYERBOT_CHEST_PREVIEW_MAX_LINES = 400;
static const int PLAYERBOT_CHEST_PREVIEW_MAX_DEPTH = 4;
static const int PLAYERBOT_MOB_PREVIEW_RANGE = 5000;

// One line a reward. A nested group is often reached from several parents -
// Srebrna Szkatulka+ (50013) names its weapon and book groups twice - so each
// group is expanded once and an item named twice is one line with the bigger
// count; the first version sent 150 lines for that chest, a third of them
// repeats, and ran out before its end; even without them that chest holds
// more than a hundred and fifty items, so an answer stops at four hundred.
static void AddPlayerBotChestPreviewLine(std::vector<std::pair<DWORD, int> >& lines, DWORD vnum, int count)
{
	for (auto& line : lines)
	{
		if (line.first != vnum)
			continue;
		if (vnum > CSpecialItemGroup::MOB_GROUP)
		{
			line.second = MAX(line.second, count);
			return;
		}
		if (line.second == count)
			return;
	}
	if ((int) lines.size() < PLAYERBOT_CHEST_PREVIEW_MAX_LINES)
		lines.emplace_back(vnum, count);
}

// An item line is "ChestPreviewItem <vnum>|<count>"; what
// CHARACTER::GiveItemFromSpecialItemGroup does instead of handing out an item
// (yang, experience, a monster, an affect) is "ChestPreviewEffect
// <kind>|<amount>". A monster's or a group's number, and the poison's nothing,
// stand where an amount stands there, so those go out as one; an item this
// process does not know is one the chest cannot give.
static void CollectPlayerBotChestPreviewGroup(DWORD groupVnum, std::set<DWORD>& seen, int depth, std::vector<std::pair<DWORD, int> >& lines)
{
	if (depth > PLAYERBOT_CHEST_PREVIEW_MAX_DEPTH || !seen.insert(groupVnum).second)
		return;
	const CSpecialItemGroup* group = ITEM_MANAGER::instance().GetSpecialItemGroup(groupVnum);
	if (!group)
		return;
	for (const auto& reward : group->GetItems())
	{
		if (reward.isSpecial)
			CollectPlayerBotChestPreviewGroup(reward.vnum, seen, depth + 1, lines);
		else if (reward.count <= 0)
			continue;
		else if (reward.vnum > CSpecialItemGroup::MOB_GROUP)
		{
			if (ITEM_MANAGER::instance().GetTable(reward.vnum))
				AddPlayerBotChestPreviewLine(lines, reward.vnum, reward.count);
		}
		else if (reward.vnum >= CSpecialItemGroup::GOLD)
		{
			const bool amount = reward.vnum == CSpecialItemGroup::GOLD || reward.vnum == CSpecialItemGroup::EXP ||
				reward.vnum == CSpecialItemGroup::SLOW || reward.vnum == CSpecialItemGroup::DRAIN_HP;
			AddPlayerBotChestPreviewLine(lines, reward.vnum, amount ? reward.count : 1);
		}
	}
}

// "/chest_preview <cell>": the chest in that cell of the asker's own bag.
ACMD(do_chest_preview)
{
	char arg1[32];
	one_argument(argument, arg1, sizeof(arg1));
	int cell = -1;
	if (!*arg1 || !str_to_number(cell, arg1) || cell < 0 || cell >= ch->GetInventoryMaxCount())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "ChestPreviewError invalid");
		return;
	}
	LPITEM box = ch->GetInventoryItem(cell);
	if (!box || (box->GetType() != ITEM_GIFTBOX && box->GetType() != ITEM_TREASURE_BOX))
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "ChestPreviewError invalid");
		return;
	}
	const DWORD vnum = box->GetVnum();
	if (!ITEM_MANAGER::instance().GetSpecialItemGroup(vnum))
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "ChestPreviewError missing");
		return;
	}
	std::vector<std::pair<DWORD, int> > lines;
	std::set<DWORD> seen;
	CollectPlayerBotChestPreviewGroup(vnum, seen, 0, lines);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ChestPreviewBegin %u", vnum);
	for (const auto& line : lines)
	{
		if (line.first > CSpecialItemGroup::MOB_GROUP)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "ChestPreviewItem %u|%d", line.first, line.second);
		else
			ch->ChatPacket(CHAT_TYPE_COMMAND, "ChestPreviewEffect %u|%d", line.first, line.second);
	}
	ch->ChatPacket(CHAT_TYPE_COMMAND, "ChestPreviewEnd %u", vnum);
}

// "/mob_drop_preview <vid>": a live monster or Metin stone on the asker's map
// and within reach, so a client cannot ask about any race it likes, nor about
// another map. The answer is ITEM_MANAGER::SendMobDropPreview. Gibon's version
// never registered this one, so the "?" asked a command nobody knew.
ACMD(do_mob_drop_preview)
{
	char arg1[32];
	one_argument(argument, arg1, sizeof(arg1));
	DWORD vid = 0;
	if (!*arg1 || !str_to_number(vid, arg1))
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobPreviewError invalid");
		return;
	}
	LPCHARACTER target = CHARACTER_MANAGER::instance().Find(vid);
	if (!target || (!target->IsMonster() && !target->IsStone()) ||
			target->GetMapIndex() != ch->GetMapIndex() ||
			DISTANCE_APPROX(target->GetX() - ch->GetX(), target->GetY() - ch->GetY()) > PLAYERBOT_MOB_PREVIEW_RANGE)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "MobPreviewError invalid");
		return;
	}
	ITEM_MANAGER::instance().SendMobDropPreview(ch, target);
}

// A person's orders to the bots of the person's guild (playerbot_guild_orders.h):
// "pomoc", "exp" or "wracajcie", sent by the guild window's buttons
// (uiguildbots.py). Where the bots go is where this character stands.
ACMD(do_gildia_boty)
{
	if (!ch || !ch->GetDesc())
		return;
	CPlayerBotManager::instance().OnGuildBotOrder(ch, argument);
}

// MT2009_PLUS_DROP_WIKI_V1: the drop wiki (client root uidropwiki.py).
//   /drop_wiki                     opens the window ("DropWiki open")
//   /drop_wiki find <i|m> <text>   items some monster drops (i) or monsters that
//                                  drop something (m) whose name holds the text
//                                  (case and Polish letters ignored; a number is
//                                  also taken as a vnum)
//   /drop_wiki show <i|m> <vnum>   which monsters drop the item / what the monster drops
// Answers, one "DropWiki <what> ..." command line each:
//   find_begin <i|m>, match <vnum> <level>, find_end <i|m> <total>
//   show_begin <i|m> <vnum>, row <vnum> <countMin> <countMax> <chance> <level>, show_end <i|m> <vnum>
//   error <why>
// chance is per kill in hundred-millionths (100000000 = 100%), for a killer at
// the monster's level (ITEM_MANAGER::GetDropWikiRows); level is the monster's
// (0 on an item's row). A find with exactly one match shows it straight away.
// Every monster's rows are worked out at most once every 30 seconds.
namespace
{
	struct DropWikiRef
	{
		DWORD dwMob;
		TDropWikiRow row;
	};

	struct DropWikiData
	{
		DWORD dwBuilt = 0;
		std::map<DWORD, std::vector<TDropWikiRow> > byMob;
		std::map<DWORD, std::vector<DropWikiRef> > byItem;
	};

	struct DropWikiMatch
	{
		int score;
		size_t length;
		DWORD vnum;
	};

	DropWikiData s_dropWiki;
	std::map<DWORD, DWORD> s_dropWikiLastAsk;
	const DWORD DROP_WIKI_REBUILD_MS = 30000;
	const DWORD DROP_WIKI_ASK_MS = 250;
	const size_t DROP_WIKI_MAX_MATCHES = 60;
	const size_t DROP_WIKI_MAX_ITEM_ROWS = 300;

	void DropWikiBuild()
	{
		const DWORD now = get_dword_time();
		if (s_dropWiki.dwBuilt && now - s_dropWiki.dwBuilt < DROP_WIKI_REBUILD_MS)
			return;
		s_dropWiki.byMob.clear();
		s_dropWiki.byItem.clear();
		for (auto it = CMobManager::instance().begin(); it != CMobManager::instance().end(); ++it)
		{
			std::vector<TDropWikiRow> rows;
			ITEM_MANAGER::instance().GetDropWikiRows(it->first, rows);
			if (rows.empty())
				continue;
			for (const auto& row : rows)
			{
				DropWikiRef ref;
				ref.dwMob = it->first;
				ref.row = row;
				s_dropWiki.byItem[row.dwVnum].push_back(ref);
			}
			s_dropWiki.byMob[it->first].swap(rows);
		}
		s_dropWiki.dwBuilt = now ? now : 1;
	}

	// Lower case, Polish letters to plain ones - whether a name is CP1250 or UTF-8.
	std::string DropWikiFold(const char* text)
	{
		static const struct { unsigned int code; char plain; } s_aUtf8[] =
		{
			{ 0x105, 'a' }, { 0x104, 'a' }, { 0x107, 'c' }, { 0x106, 'c' }, { 0x119, 'e' }, { 0x118, 'e' },
			{ 0x142, 'l' }, { 0x141, 'l' }, { 0x144, 'n' }, { 0x143, 'n' }, { 0x0F3, 'o' }, { 0x0D3, 'o' },
			{ 0x15B, 's' }, { 0x15A, 's' }, { 0x17A, 'z' }, { 0x179, 'z' }, { 0x17C, 'z' }, { 0x17B, 'z' },
		};
		static const struct { unsigned char code; char plain; } s_aCp1250[] =
		{
			{ 0xB9, 'a' }, { 0xA5, 'a' }, { 0xE6, 'c' }, { 0xC6, 'c' }, { 0xEA, 'e' }, { 0xCA, 'e' },
			{ 0xB3, 'l' }, { 0xA3, 'l' }, { 0xF1, 'n' }, { 0xD1, 'n' }, { 0xF3, 'o' }, { 0xD3, 'o' },
			{ 0x9C, 's' }, { 0x8C, 's' }, { 0x9F, 'z' }, { 0x8F, 'z' }, { 0xBF, 'z' }, { 0xAF, 'z' },
		};
		std::string out;
		const unsigned char* p = (const unsigned char*) (text ? text : "");
		while (*p)
		{
			if (*p < 0x80)
			{
				out += (char) tolower(*p);
				++p;
				continue;
			}
			if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80)
			{
				const unsigned int code = ((*p & 0x1F) << 6) | (p[1] & 0x3F);
				char plain = 0;
				for (const auto& letter : s_aUtf8)
					if (letter.code == code)
					{
						plain = letter.plain;
						break;
					}
				if (plain)
				{
					out += plain;
					p += 2;
					continue;
				}
			}
			char plain = 0;
			for (const auto& letter : s_aCp1250)
				if (letter.code == *p)
				{
					plain = letter.plain;
					break;
				}
			out += plain ? plain : (char) *p;
			++p;
		}
		return out;
	}

	// 0 the whole name, 1 its start, 2 a word's start, 3 anywhere; -1 not in it.
	int DropWikiScore(const std::string& name, const std::string& query)
	{
		const size_t pos = name.find(query);
		if (pos == std::string::npos)
			return -1;
		if (name == query)
			return 0;
		if (pos == 0)
			return 1;
		if (name[pos - 1] == ' ' || name[pos - 1] == '(' || name[pos - 1] == '-')
			return 2;
		return 3;
	}

	unsigned int DropWikiChance(double chance)
	{
		if (chance <= 0.0)
			return 0;
		if (chance >= 1.0)
			return 100000000;
		const unsigned int value = (unsigned int) (chance * 100000000.0 + 0.5);
		return value ? value : 1;
	}

	int DropWikiMobLevel(DWORD race)
	{
		const CMob* mob = CMobManager::instance().Get(race);
		return mob ? mob->m_table.bLevel : 0;
	}

	void DropWikiShow(LPCHARACTER ch, bool itemMode, DWORD vnum)
	{
		const char kind = itemMode ? 'i' : 'm';
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki show_begin %c %u", kind, vnum);
		if (itemMode)
		{
			auto it = s_dropWiki.byItem.find(vnum);
			if (it != s_dropWiki.byItem.end())
			{
				std::vector<DropWikiRef> refs = it->second;
				std::sort(refs.begin(), refs.end(), [](const DropWikiRef& a, const DropWikiRef& b)
				{
					if (a.row.dChance != b.row.dChance)
						return a.row.dChance > b.row.dChance;
					return a.dwMob < b.dwMob;
				});
				if (refs.size() > DROP_WIKI_MAX_ITEM_ROWS)
					refs.resize(DROP_WIKI_MAX_ITEM_ROWS);
				for (const auto& ref : refs)
					ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki row %u %d %d %u %d", ref.dwMob,
						ref.row.iCountMin, ref.row.iCountMax, DropWikiChance(ref.row.dChance), DropWikiMobLevel(ref.dwMob));
			}
		}
		else
		{
			auto it = s_dropWiki.byMob.find(vnum);
			if (it != s_dropWiki.byMob.end())
				for (const auto& row : it->second)
					ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki row %u %d %d %u 0", row.dwVnum,
						row.iCountMin, row.iCountMax, DropWikiChance(row.dChance));
		}
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki show_end %c %u", kind, vnum);
	}

	void DropWikiFind(LPCHARACTER ch, bool itemMode, const char* text)
	{
		std::string query = DropWikiFold(text);
		while (!query.empty() && query[query.size() - 1] == ' ')
			query.erase(query.size() - 1);
		bool numeric = !query.empty();
		for (size_t i = 0; i < query.size(); ++i)
			if (query[i] < '0' || query[i] > '9')
				numeric = false;
		if (query.size() < 2 && !numeric)
		{
			ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki error short");
			return;
		}
		const DWORD number = numeric && query.size() <= 9 ? (DWORD) strtoul(query.c_str(), NULL, 10) : 0;

		std::vector<DropWikiMatch> found;
		if (itemMode)
		{
			for (const auto& entry : s_dropWiki.byItem)
			{
				const TItemTable* table = ITEM_MANAGER::instance().GetTable(entry.first);
				if (!table)
					continue;
				const std::string name = DropWikiFold(table->szLocaleName);
				const int score = entry.first == number ? 0 : DropWikiScore(name, query);
				if (score >= 0)
					found.push_back({ score, name.size(), entry.first });
			}
		}
		else
		{
			for (const auto& entry : s_dropWiki.byMob)
			{
				const CMob* mob = CMobManager::instance().Get(entry.first);
				if (!mob)
					continue;
				const std::string name = DropWikiFold(mob->m_table.szLocaleName);
				const int score = entry.first == number ? 0 : DropWikiScore(name, query);
				if (score >= 0)
					found.push_back({ score, name.size(), entry.first });
			}
		}
		std::sort(found.begin(), found.end(), [](const DropWikiMatch& a, const DropWikiMatch& b)
		{
			if (a.score != b.score)
				return a.score < b.score;
			if (a.length != b.length)
				return a.length < b.length;
			return a.vnum < b.vnum;
		});

		const char kind = itemMode ? 'i' : 'm';
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki find_begin %c", kind);
		for (size_t i = 0; i < found.size() && i < DROP_WIKI_MAX_MATCHES; ++i)
			ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki match %u %d", found[i].vnum,
				itemMode ? 0 : DropWikiMobLevel(found[i].vnum));
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki find_end %c %u", kind, (unsigned int) found.size());
		if (found.size() == 1)
			DropWikiShow(ch, itemMode, found[0].vnum);
	}
}

ACMD(do_drop_wiki)
{
	if (!ch || !ch->GetDesc())
		return;

	char sub[16], kind[8];
	const char* rest = one_argument(argument, sub, sizeof(sub));
	if (!*sub)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki open");
		return;
	}
	rest = one_argument(rest, kind, sizeof(kind));
	if ((kind[0] != 'i' && kind[0] != 'm') || kind[1] || !rest)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki error bad");
		return;
	}
	const bool itemMode = kind[0] == 'i';

	const DWORD now = get_dword_time();
	DWORD& last = s_dropWikiLastAsk[ch->GetPlayerID()];
	if (last && now - last < DROP_WIKI_ASK_MS)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki error busy");
		return;
	}
	last = now;

	DropWikiBuild();
	if (!strcmp(sub, "find"))
		DropWikiFind(ch, itemMode, rest);
	else if (!strcmp(sub, "show"))
	{
		DWORD vnum = 0;
		str_to_number(vnum, rest);
		DropWikiShow(ch, itemMode, vnum);
	}
	else
		ch->ChatPacket(CHAT_TYPE_COMMAND, "DropWiki error bad");
}

//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top

