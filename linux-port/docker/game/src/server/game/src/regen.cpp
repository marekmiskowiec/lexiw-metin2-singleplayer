#include "stdafx.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "regen.h"
#include "mob_manager.h"
#include "dungeon.h"
#include "utils.h"
#include "questmanager.h"
#include "mob_manager.h"
#include "constants.h"

LPREGEN	regen_list = NULL;
LPREGEN_EXCEPTION regen_exception_list = NULL;

enum ERegenModes
{
	MODE_TYPE = 0,
	MODE_SX,
	MODE_SY,
	MODE_EX,
	MODE_EY,
	MODE_Z_SECTION,
	MODE_DIRECTION,
	MODE_REGEN_TIME,
	MODE_REGEN_PERCENT,
	MODE_MAX_COUNT,
	MODE_VNUM
};

static bool get_word(FILE *fp, char *buf)
{
	int i = 0;
	int c;

	int semicolon_mode = 0;

	while ((c = fgetc(fp)) != EOF)
	{
		if (i == 0)
		{
			if (c == '"')
			{
				semicolon_mode = 1;
				continue;
			}

			if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
				continue;
		}

		if (semicolon_mode)
		{
			if (c == '"')
			{
				buf[i] = '\0';
				return true;
			}

			buf[i++] = c;
		}
		else
		{
			if ((c == ' ' || c == '\t' || c == '\n' || c == '\r'))
			{
				buf[i] = '\0';
				return true;
			}

			buf[i++] = c;
		}

		if (i == 2 && buf[0] == '/' && buf[1] == '/')
		{
			buf[i] = '\0';
			return true;
		}
	}

	buf[i] = '\0';
	return (i != 0);
}

static void next_line(FILE *fp)
{
	int c;

	while ((c = fgetc(fp)) != EOF)
		if (c == '\n')
			return;
}

// playerbot: every monster a respawn line can put on the map - the vnum
// of a single line, every member of a group (its leader too), every
// member of every group a group of groups may draw. The boss test below
// asked a group and never a group of groups, and stone.txt writes its ore
// veins and herbs that way, as a few maps write their Metin stones.
static void regen_member_vnums(LPREGEN regen, std::vector<DWORD>& vnums)
{
	vnums.clear();
	if (regen->type == REGEN_TYPE_GROUP)
	{
		CMobGroup* pkGroup = CMobManager::instance().GetGroup(regen->vnum);
		if (pkGroup)
			vnums = pkGroup->GetMemberVector();
	}
	else if (regen->type == REGEN_TYPE_GROUP_GROUP)
	{
		const std::vector<DWORD>* groups = CMobManager::instance().GetGroupGroupMembers(regen->vnum);
		if (!groups)
			return;
		for (DWORD dwGroup : *groups)
		{
			CMobGroup* pkGroup = CMobManager::instance().GetGroup(dwGroup);
			if (pkGroup)
				vnums.insert(vnums.end(), pkGroup->GetMemberVector().begin(), pkGroup->GetMemberVector().end());
		}
	}
	else if (regen->type == REGEN_TYPE_MOB || regen->type == REGEN_TYPE_ANYWHERE)
		vnums.push_back(regen->vnum);
}

static bool read_line(FILE *fp, LPREGEN regen)
{
	char szTmp[256];

	int mode = MODE_TYPE;
	int tmpTime;
	DWORD i;

	while (get_word(fp, szTmp))
	{
		if (!strncmp(szTmp, "//", 2))
		{
			next_line(fp);
			continue;
		}

		switch (mode)
		{
			case MODE_TYPE:
				if (szTmp[0] == 'm')
					regen->type = REGEN_TYPE_MOB;
				else if (szTmp[0] == 'g')
					regen->type = REGEN_TYPE_GROUP;
				else if (szTmp[0] == 'e')
					regen->type = REGEN_TYPE_EXCEPTION;
				else if (szTmp[0] == 'r')
					regen->type = REGEN_TYPE_GROUP_GROUP;
				else if (szTmp[0] == 's')
					regen->type = REGEN_TYPE_ANYWHERE;
				else
				{
					sys_err("read_line: unknown regen type %c", szTmp[0]);
					exit(1);
				}

				if (szTmp[1] == 'a') //@fixme195
					regen->is_aggressive = true;
				else if (szTmp[1] == 'h')
					regen->is_high_traffic_hours = true;
				else if (szTmp[1] == 'd')
					regen->is_first_spawn_delayed = true;

				++mode;
				break;

			case MODE_SX:
				str_to_number(regen->sx, szTmp);
				++mode;
				break;

			case MODE_SY:
				str_to_number(regen->sy, szTmp);
				++mode;
				break;

			case MODE_EX:
				{
					int iX = 0;
					str_to_number(iX, szTmp);

					regen->sx -= iX;
					regen->ex = regen->sx + iX * 2;

					regen->sx *= 100;
					regen->ex *= 100;

					++mode;
				}
				break;

			case MODE_EY:
				{
					int iY = 0;
					str_to_number(iY, szTmp);

					regen->sy -= iY;
					regen->ey = regen->sy + iY * 2;

					regen->sy *= 100;
					regen->ey *= 100;

					++mode;
				}
				break;

			case MODE_Z_SECTION:
				str_to_number(regen->z_section, szTmp);

				if (regen->type == REGEN_TYPE_EXCEPTION)
					return true;

				++mode;
				break;

			case MODE_DIRECTION:
				str_to_number(regen->direction, szTmp);
				++mode;
				break;

			case MODE_REGEN_TIME:
				{
					regen->time = 0;
					regen->endTime = 0;
					tmpTime = 0;

					bool parsingEndTime = false;

					for (i = 0; i < strlen(szTmp); ++i)
					{
						char ch = szTmp[i];

						if (ch == '-') {
							// Switch to parsing endTime
							regen->time += tmpTime; // Save the first value
							tmpTime = 0; // Reset tmpTime for the next value
							parsingEndTime = true;
						}
						else if (ch == 'h' || ch == 'm' || ch == 's') {
							// Add to the appropriate unit
							int multiplier = (ch == 'h') ? 3600 : (ch == 'm') ? 60 : 1;
							tmpTime *= multiplier;

							if (parsingEndTime) {
								regen->endTime += tmpTime;
							}
							else {
								regen->time += tmpTime;
							}

							tmpTime = 0; // Reset tmpTime for the next segment
						}
						else if (std::isdigit(ch)) {
							tmpTime = tmpTime * 10 + (ch - '0');
						}
					}

					if (tmpTime > 0) {
						if (parsingEndTime) {
							regen->endTime += tmpTime;
						}
						else {
							regen->time += tmpTime;
						}
					}

					++mode;
				}
				break;
			case MODE_REGEN_PERCENT:
				++mode;
				break;

			case MODE_MAX_COUNT:
				regen->count = 0;
				str_to_number(regen->max_count, szTmp);
				++mode;
				break;

			case MODE_VNUM:
			{
				// playerbot: the vnum is read before it is asked about. The boss
				// test looked at the zero of a fresh REGEN, so no line was ever a
				// boss or a stone and fastBossSpawn (the /rates page's "Metiny i
				// bossowie") reached nothing; and a group of groups is asked the
				// way a group is - a boss or a stone among what it may put down.
				str_to_number(regen->vnum, szTmp);
				std::vector<DWORD> vnums;
				regen_member_vnums(regen, vnums);
				for (DWORD mobVnum : vnums)
				{
					const CMob* pkMob = CMobManager::instance().Get(mobVnum);
					if (pkMob && (pkMob->m_table.bRank >= MOB_RANK_BOSS || IsMiniBoss(mobVnum)))
					{
						regen->is_boss_or_stone = true;
						break;
					}
				}
				++mode;
			}
				return true;
		}
	}

	return false;
}

bool is_regen_exception(long x, long y)
{
	LPREGEN_EXCEPTION exc;

	for (exc = regen_exception_list; exc; exc = exc->next)
	{
		if (exc->sx <= x && exc->sy <= y)
			if (exc->ex >= x && exc->ey >= y)
				return true;
	}

	return false;
}

static void regen_spawn_dungeon(LPREGEN regen, LPDUNGEON pDungeon, bool bOnce)
{
	DWORD	num;
	DWORD	i;

	num = (regen->max_count - regen->count);

	if (!num)
		return;

	for (i = 0; i < num; ++i)
	{
		LPCHARACTER ch = NULL;

		if (regen->type == REGEN_TYPE_ANYWHERE)
		{
			ch = CHARACTER_MANAGER::instance().SpawnMobRandomPosition(regen->vnum, regen->lMapIndex, regen->is_aggressive); //@fixme195

			if (ch)
			{
				++regen->count;
				ch->SetDungeon(pDungeon);
			}
		}
		else if (regen->sx == regen->ex && regen->sy == regen->ey)
		{
			ch = CHARACTER_MANAGER::instance().SpawnMob(regen->vnum,
					regen->lMapIndex,
					regen->sx,
					regen->sy,
					regen->z_section,
					false,
					regen->direction == 0 ? number(0, 7) * 45 : (regen->direction - 1) * 45);

			if (ch)
			{
				++regen->count;
				ch->SetDungeon(pDungeon);
			}
		}
		else
		{
			if (regen->type == REGEN_TYPE_MOB)
			{
				ch = CHARACTER_MANAGER::Instance().SpawnMobRange(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, true, false, regen->is_aggressive); // @fixme195

				if (ch)
				{
					++regen->count;
					ch->SetDungeon(pDungeon);
				}
			}
			else if (regen->type == REGEN_TYPE_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? NULL : regen, regen->is_aggressive, pDungeon))
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroupGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? NULL : regen, regen->is_aggressive, pDungeon))
					++regen->count;
			}
		}

		if (ch && !bOnce)
			ch->SetRegen(regen);
	}
}

// playerbot: how many a respawn line keeps standing - its own count times
// the operator's multiplier (m2_boss_count for the lines of bosses and
// Metin stones, m2_mob_count for the rest; a percent, 100 or unset being
// the line as written, 400 the most). Only a line all of whose monsters
// are monsters or stones is multiplied: an NPC, a portal or a shop keeper
// stays as written, and so do the groups of ore veins, herbs and horses
// that stone.txt and npc.txt carry. Nor is a spawn a quest asks for once;
// the dungeons keep their own regen_spawn_dungeon and are not touched.
static DWORD regen_target_count(LPREGEN regen, bool bOnce)
{
	if (regen->max_count <= 0)
		return 0;
	if (bOnce)
		return regen->max_count;
	// MT2009_PLUS_REGEN_COUNT_NO_DUNGEON_V1: the panel's x2-x4 for Metins,
	// bosses and monsters stays out of the dungeons - an instance (its index
	// 10000 and up) or a map a dungeon runs on keeps its lines as written.
	if (regen->lMapIndex >= 10000 || CDungeonManager::instance().FindByMapIndex(regen->lMapIndex))
		return regen->max_count;
	const int percent = quest::CQuestManager::instance().GetEventFlag(regen->is_boss_or_stone ? "m2_boss_count" : "m2_mob_count");
	if (percent <= 100)
		return regen->max_count;
	std::vector<DWORD> vnums;
	regen_member_vnums(regen, vnums);
	if (vnums.empty())
		return regen->max_count;
	for (DWORD mobVnum : vnums)
	{
		const CMob* pkMob = CMobManager::instance().Get(mobVnum);
		if (!pkMob || (pkMob->m_table.bType != CHAR_TYPE_MONSTER && pkMob->m_table.bType != CHAR_TYPE_STONE))
			return regen->max_count;
	}
	return (DWORD)regen->max_count * (DWORD)MIN(percent, 400) / 100;
}

static void regen_spawn(LPREGEN regen, bool bOnce)
{
	DWORD	num;
	DWORD	i;

	// playerbot: up to the target; a count above it (the multiplier was
	// lowered while the extra ones still stand) spawns nothing, where
	// max_count - count would have wrapped round to four billion.
	const DWORD target = regen_target_count(regen, bOnce);
	if (regen->count < 0 || (DWORD)regen->count >= target)
		return;
	num = target - (DWORD)regen->count;

	for (i = 0; i < num; ++i)
	{
		LPCHARACTER ch = NULL;

		if (regen->is_high_traffic_hours && !is_time_between_hours(17, 22))
		{
			continue;
		}

		if (regen->type == REGEN_TYPE_ANYWHERE)
		{
			ch = CHARACTER_MANAGER::instance().SpawnMobRandomPosition(regen->vnum, regen->lMapIndex, regen->is_aggressive); //@fixme195

			if (ch)
				++regen->count;
		}
		else if (regen->sx == regen->ex && regen->sy == regen->ey)
		{
			ch = CHARACTER_MANAGER::instance().SpawnMob(regen->vnum,
					regen->lMapIndex,
					regen->sx,
					regen->sy,
					regen->z_section,
					false,
					regen->direction == 0 ? number(0, 7) * 45 : (regen->direction - 1) * 45);

			if (ch)
				++regen->count;
		}
		else
		{
			if (regen->type == REGEN_TYPE_MOB)
			{
				ch = CHARACTER_MANAGER::Instance().SpawnMobRange(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, true, regen->is_aggressive, regen->is_aggressive );

				if (ch)
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? NULL : regen, regen->is_aggressive))
					++regen->count;
			}
			else if (regen->type == REGEN_TYPE_GROUP_GROUP)
			{
				if (CHARACTER_MANAGER::Instance().SpawnGroupGroup(regen->vnum, regen->lMapIndex, regen->sx, regen->sy, regen->ex, regen->ey, bOnce ? NULL : regen, regen->is_aggressive))
					++regen->count;
			}
		}

		if (ch && !bOnce)
			ch->SetRegen(regen);
	}
}

EVENTFUNC(dungeon_regen_event)
{
	dungeon_regen_event_info* info = dynamic_cast<dungeon_regen_event_info*>( event->info );

	if ( info == NULL )
	{
		sys_err( "dungeon_regen_event> <Factor> Null pointer" );
		return 0;
	}

	LPDUNGEON pDungeon = CDungeonManager::instance().Find(info->dungeon_id);
	if (pDungeon == NULL) {
		return 0;
	}

	LPREGEN	regen = info->regen;
	if (regen->time == 0)
	{
		regen->event = NULL;
	}

	regen_spawn_dungeon(regen, pDungeon, false);
	return PASSES_PER_SEC(regen->time);
}

bool regen_do(const char* filename, long lMapIndex, int base_x, int base_y, LPDUNGEON pDungeon, bool bOnce)
{
	if (g_bNoRegen)
		return true;

	if ( lMapIndex >= 114 && lMapIndex <= 117 )
		return true;

	LPREGEN regen = NULL;
	FILE* fp = fopen(filename, "rt");

	if (NULL == fp)
	{
		sys_err("SYSTEM: regen_do: %s: file not found", filename);
		return false;
	}

	while (true)
	{
		REGEN tmp{};

		if (!read_line(fp, &tmp))
			break;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			if (!bOnce)
			{
				regen = M2_NEW REGEN;
				*regen = tmp;
			}
			else
				regen = &tmp;

			if (pDungeon)
				regen->is_aggressive = true;

			regen->is_high_traffic_hours = false;
			regen->lMapIndex = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const CMob * p = CMobManager::instance().Get(regen->vnum);

				if (!p)
				{
					sys_err("In %s, No mob data by vnum %u", filename, regen->vnum);
					if (!bOnce) {
						M2_DELETE(regen);
					}
					continue;
				}
			}

			if (!bOnce && pDungeon != NULL)
			{
				dungeon_regen_event_info* info = AllocEventInfo<dungeon_regen_event_info>();

				info->regen = regen;
				info->dungeon_id = pDungeon->GetId();

				regen->event = event_create(dungeon_regen_event, info, PASSES_PER_SEC(regen->GetInitialSpawnDelay()) + PASSES_PER_SEC(regen->GetSpawnDelay()));

				pDungeon->AddRegen(regen);
				// regen_id should be determined at this point,
				// before the call to CHARACTER::SetRegen()
			}

			regen_spawn_dungeon(regen, pDungeon, bOnce);

		}
	}

	fclose(fp);
	return true;
}

// MT2009_PLUS_EASTER_METIN_CAP_V1 (server-patches/playerqol): the Easter
// metins (map/easter/metin_regen_level*.txt, put down one at a time by the
// event quest's regen_in_map on a map of the killer's kingdom and level) never
// had a ceiling. Every kill of one put one or more new ones down - a killer 15
// levels above it always did - and the package compiled three generations of
// the event (event_easter, _2012, _2013; the Dockerfile now drops the two old
// ones) that all answered the same kill, so with bots farming them the
// population only grew: "Metin na Metinie" on every map, worse with the
// panel's faster Metin respawn (a player, 29 September). Now a map holds at
// most EASTER_METINS_PER_MAP of them alive, times the panel's Metin setting as
// the map regens feel it: the count multiplier (m2_boss_count, 100-400%) and
// the respawn time (fastBossSpawn<map>, else fastBossSpawn: 65 = every 65% of
// the time = 100/65 as many), 4x at most. The fraction goes to a fixed share
// of the maps (65%: 3 * 1.54 = 4.6 - 4 on some maps, 5 on six in ten), so the
// world holds that many times the Easter metins. Nothing is put down once the
// event is off, where a kill used to go on spawning them for ever.
static const int EASTER_METINS_PER_MAP = 3;

static int EasterMetinCap(long lMapIndex)
{
	quest::CQuestManager& q = quest::CQuestManager::instance();
	const int count = MINMAX(100, q.GetEventFlag("m2_boss_count"), 400);
	int delay = MINMAX(0, q.GetEventFlag(("fastBossSpawn" + std::to_string(lMapIndex)).c_str()), 100);
	if (delay == 0)
		delay = MINMAX(0, q.GetEventFlag("fastBossSpawn"), 100);
	if (delay < 10)
		delay = 100; // 0 = untouched; the panels go down to 10
	// the multiplier in percent: count% * (100 / delay%), 100..400
	const int mult = MINMAX(100, count * 100 / delay, 400);
	const int capX100 = EASTER_METINS_PER_MAP * mult;
	return capX100 / 100 + ((lMapIndex * 37 + 11) % 100 < capX100 % 100 ? 1 : 0);
}

static bool EasterRegenMaySpawn(LPREGEN regen)
{
	if (quest::CQuestManager::instance().GetEventFlag("easter_drop") <= 0)
		return false;
	// The character manager keeps a list by race only for the races it was
	// told to (the NPCs); the Easter metins join it before the first is put
	// down, so every one of them is on it.
	static bool s_bRegistered = false;
	if (!s_bRegistered)
	{
		for (DWORD dwVnum = 8041; dwVnum <= 8050; ++dwVnum)
			CHARACTER_MANAGER::instance().RegisterRaceNum(dwVnum);
		s_bRegistered = true;
	}
	const int cap = EasterMetinCap(regen->lMapIndex);
	int alive = 0;
	for (DWORD dwVnum = 8041; dwVnum <= 8050; ++dwVnum)
	{
		CharacterVectorInteractor i;
		if (CHARACTER_MANAGER::instance().GetCharactersByRaceNum(dwVnum, i))
			for (CharacterVectorInteractor::iterator it = i.begin(); it != i.end(); ++it)
				if (*it && (*it)->GetMapIndex() == regen->lMapIndex && !(*it)->IsDead())
					++alive;
	}
	if (alive >= cap)
	{
		sys_log(0, "EASTER_METIN_CAP: map %ld vnum %u alive %d cap %d - not spawned", regen->lMapIndex, regen->vnum, alive, cap);
		return false;
	}
	regen->max_count = MIN(MAX(1, regen->max_count), cap - alive);
	sys_log(0, "EASTER_METIN_CAP: map %ld vnum %u alive %d cap %d - spawning %d", regen->lMapIndex, regen->vnum, alive, cap, regen->max_count);
	return true;
}

bool regen_load_in_file(const char* filename, long lMapIndex, int base_x, int base_y)
{
	// MT2009_PLUS_EASTER_METIN_CAP_V1: see EasterRegenMaySpawn above.
	const bool bEasterMetins = filename && strstr(filename, "/map/easter/") != NULL;
	if (g_bNoRegen)
		return true;

	LPREGEN regen = NULL;
	FILE * fp = fopen(filename, "rt");

	if (NULL == fp)
	{
		sys_err("SYSTEM: regen_do: %s: file not found", filename);
		return false;
	}

	while (true)
	{
		REGEN tmp{};

		if (!read_line(fp, &tmp))
			break;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			regen = &tmp;

			regen->is_aggressive = true;
			regen->is_high_traffic_hours = false;

			regen->lMapIndex = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const CMob * p = CMobManager::instance().Get(regen->vnum);

				if (!p)
				{
					sys_err("In %s, No mob data by vnum %u", filename, regen->vnum);
					continue;
				}
			}

			// MT2009_PLUS_EASTER_METIN_CAP_V1 (spawn)
			if (bEasterMetins && !EasterRegenMaySpawn(regen))
				continue;

			regen_spawn(regen, true);
		}
	}

	fclose(fp);
	return true;
}

EVENTFUNC(regen_event)
{
	regen_event_info* info = dynamic_cast<regen_event_info*>( event->info );

	if ( info == NULL )
	{
		sys_err( "regen_event> <Factor> Null pointer" );
		return 0;
	}

	LPREGEN	regen = info->regen;

	if (regen->time == 0)
		regen->event = NULL;

	regen_spawn(regen, false);

	DWORD spawnDelay = regen->GetSpawnDelay();

	std::string flagName = "fastMobSpawn";
	if (regen->is_boss_or_stone)
	{
		flagName = "fastBossSpawn";
	}
	flagName += std::to_string(info->regen->lMapIndex);

	int flagValue = MINMAX(0, quest::CQuestManager::instance().GetEventFlag(flagName.c_str()), 100);
	// playerbot: the map-less flag is the world-wide fallback (the classic
	// panel's /rates page); a per-map flag, when set, still wins.
	if (flagValue == 0)
		flagValue = MINMAX(0, quest::CQuestManager::instance().GetEventFlag(regen->is_boss_or_stone ? "fastBossSpawn" : "fastMobSpawn"), 100);
	if (flagValue)
	{
		spawnDelay = MAX(3, spawnDelay * flagValue / 100);
	}

	return PASSES_PER_SEC(spawnDelay);
}

bool regen_load(const char* filename, long lMapIndex, int base_x, int base_y)
{
	if (g_bNoRegen)
		return true;

	LPREGEN regen = NULL;
	FILE* fp = fopen(filename, "rt");

	if (NULL == fp)
	{
		sys_log(0, "SYSTEM: regen_load: %s: file not found", filename);
		return false;
	}

	while (true)
	{
		REGEN tmp{};

		if (!read_line(fp, &tmp))
			break;

		if (tmp.type == REGEN_TYPE_MOB ||
			tmp.type == REGEN_TYPE_GROUP ||
			tmp.type == REGEN_TYPE_GROUP_GROUP ||
			tmp.type == REGEN_TYPE_ANYWHERE)
		{
			if (test_server)
			{
				CMobManager::instance().IncRegenCount(tmp.type, tmp.vnum, tmp.max_count, tmp.time);
			}

			regen = M2_NEW REGEN;
			*regen = tmp;
			INSERT_TO_TW_LIST(regen, regen_list, prev, next);

			regen->lMapIndex = lMapIndex;
			regen->count = 0;

			regen->sx += base_x;
			regen->ex += base_x;

			regen->sy += base_y;
			regen->ey += base_y;

			if (regen->sx > regen->ex)
			{
				regen->sx ^= regen->ex;
				regen->ex ^= regen->sx;
				regen->sx ^= regen->ex;
			}

			if (regen->sy > regen->ey)
			{
				regen->sy ^= regen->ey;
				regen->ey ^= regen->sy;
				regen->sy ^= regen->ey;
			}

			if (regen->type == REGEN_TYPE_MOB)
			{
				const CMob * p = CMobManager::instance().Get(regen->vnum);

				if (!p)
				{
					sys_err("In %s, No mob data by vnum %u", filename, regen->vnum);
				}
				else if (p->m_table.bType == CHAR_TYPE_NPC || p->m_table.bType == CHAR_TYPE_WARP || p->m_table.bType == CHAR_TYPE_GOTO)
				{
					bool isHiddenNpc = p->m_table.dwVnum == 20403;
					if (!isHiddenNpc)
					{
						SECTREE_MANAGER::instance().InsertNPCPosition(lMapIndex,
							p->m_table.bType,
							p->m_table.szLocaleName,
							(regen->sx + regen->ex) / 2 - base_x,
							(regen->sy + regen->ey) / 2 - base_y);
					}
				}
			}

			//NO_REGEN

			if (regen->time != 0)
			{
				regen_event_info* info = AllocEventInfo<regen_event_info>();
				info->regen = regen;

				DWORD regenEventDelay = 0;
				if (regen->is_first_spawn_delayed)
					regenEventDelay = regen->GetInitialSpawnDelay();
				else
				{
					regenEventDelay = regen->GetSpawnDelay();
					regen_spawn(regen, false);
				}

				regen->event = event_create(regen_event, info, PASSES_PER_SEC(regenEventDelay));
			}
			//END_NO_REGEN
		}
		else if (tmp.type == REGEN_TYPE_EXCEPTION)
		{
			LPREGEN_EXCEPTION exc;

			exc = M2_NEW REGEN_EXCEPTION;

			exc->sx = tmp.sx;
			exc->sy = tmp.sy;
			exc->ex = tmp.ex;
			exc->ey = tmp.ey;
			exc->z_section = tmp.z_section;

			INSERT_TO_TW_LIST(exc, regen_exception_list, prev, next);
		}
	}

	fclose(fp);
	return true;
}

void regen_free(void)
{
	LPREGEN		regen, next_regen;
	LPREGEN_EXCEPTION	exc, next_exc;

	for (regen = regen_list; regen; regen = next_regen)
	{
		next_regen = regen->next;

		event_cancel(&regen->event);
		M2_DELETE(regen);
	}

	regen_list = NULL;

	for (exc = regen_exception_list; exc; exc = next_exc)
	{
		next_exc = exc->next;

		M2_DELETE(exc);
	}

	regen_exception_list = NULL;
}

void regen_reset(int x, int y)
{
	LPREGEN regen;

	for (regen = regen_list; regen; regen = regen->next)
	{
		if (!regen->event)
			continue;

		if (x != 0 || y != 0)
		{
			if (x >= regen->sx && x <= regen->ex)
				if (y >= regen->sy && y <= regen->ey)
					event_reset_time(regen->event, 1);
		}

		else
			event_reset_time(regen->event, 1);
	}
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
