#include "stdafx.h"
#include "config.h"

#include "BlueDragon.h"

#include "vector.h"
#include "utils.h"
#include "char.h"
#include "mob_manager.h"
#include "sectree_manager.h"
#include "battle.h"
#include "affect.h"
#include "BlueDragon_Binder.h"
#include "BlueDragon_Skill.h"
#include "packet.h"
#include "motion.h"

time_t UseBlueDragonSkill(LPCHARACTER pChar, unsigned int idx)
{
	LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap( pChar->GetMapIndex() );

	if (NULL == pSecMap)
		return 0;

	int nextUsingTime = 0;

	switch (idx)
	{
		case 0:
			{
				sys_log(0, "BlueDragon: Using Skill Breath");

				FSkillBreath f(pChar);

				pSecMap->for_each( f );

				nextUsingTime = number(BlueDragon_GetSkillFactor(3, "Skill0", "period", "min"), BlueDragon_GetSkillFactor(3, "Skill0", "period", "max"));
			}
			break;

		case 1:
			{
				sys_log(0, "BlueDragon: Using Skill Weak Breath");

				FSkillWeakBreath f(pChar);

				pSecMap->for_each( f );

				nextUsingTime = number(BlueDragon_GetSkillFactor(3, "Skill1", "period", "min"), BlueDragon_GetSkillFactor(3, "Skill1", "period", "max"));
			}
			break;

		case 2:
			{
				sys_log(0, "BlueDragon: Using Skill EarthQuake");

				FSkillEarthQuake f(pChar);

				pSecMap->for_each( f );

				nextUsingTime = number(BlueDragon_GetSkillFactor(3, "Skill2", "period", "min"), BlueDragon_GetSkillFactor(3, "Skill2", "period", "max"));

				if (NULL != f.pFarthestChar)
				{
					pChar->BeginFight( f.pFarthestChar );
				}
			}
			break;

		default:
			sys_err("BlueDragon: Wrong Skill Index: %d", idx);
			return 0;
	}

	int addPct = BlueDragon_GetRangeFactor("hp_period", pChar->GetHPPct());

	nextUsingTime += (nextUsingTime * addPct) / 100;

	return nextUsingTime;
}

// MT2009_PLUS_BLUE_DRAGON_V1 (block): the Blue Dragon lair (server-patches/bluedragon,
// quest/blue_dragon_lair.quest). As in Owsap's renewal (BlueDragon_Block/IsBoss): in an
// instance of map 208 (2080000-2089999) Beran-Setaou takes no damage while any of the four
// dragon stones is alive there - one pass over the instance, a dead stone no longer counts.
// The public map 208 and every other map are left as they were.
#include <array>
#include <map>
#include "char_manager.h"

static const DWORD MT2009_BLUE_DRAGON_BOSS_VNUM = 2493;
static const long MT2009_BLUE_DRAGON_MAP_INDEX = 208;

struct FMt2009PlusCountDragonStones
{
	size_t cnt;

	FMt2009PlusCountDragonStones() : cnt(0) {}

	void operator() (LPENTITY ent)
	{
		if (NULL == ent || !ent->IsType(ENTITY_CHARACTER))
			return;

		LPCHARACTER pChar = static_cast<LPCHARACTER>(ent);

		if (!pChar->IsStone() || pChar->IsDead())
			return;

		switch (pChar->GetRaceNum())
		{
			case 8031:
			case 8032:
			case 8033:
			case 8034:
				++cnt;
				break;
		}
	}
};

bool BlueDragon_IsBoss(DWORD dwVnum)
{
	return MT2009_BLUE_DRAGON_BOSS_VNUM == dwVnum;
}

bool BlueDragon_Block(long lMapIndex)
{
	if (lMapIndex < MT2009_BLUE_DRAGON_MAP_INDEX * 10000 || lMapIndex >= (MT2009_BLUE_DRAGON_MAP_INDEX + 1) * 10000)
		return false;

	LPSECTREE_MAP pSecMap = SECTREE_MANAGER::instance().GetMap(lMapIndex);

	if (NULL == pSecMap)
		return false;

	FMt2009PlusCountDragonStones f;
	pSecMap->for_each(f);

	return f.cnt > 0;
}

// MT2009_PLUS_BLUE_DRAGON_V1 (cooldown): the skills' next-use times per dragon (by VID). The
// classic code kept one static array for every dragon, so two lairs fought at the same time
// shared - and stole - each other's cooldowns. Entries of dragons that are gone are dropped
// once the table holds more than a few.
static std::array<time_t, 3>& BlueDragon_SkillCanUseTime(LPCHARACTER pChar)
{
	static std::map<DWORD, std::array<time_t, 3> > s_mapSkillCanUseTime;

	if (s_mapSkillCanUseTime.size() > 16)
	{
		for (std::map<DWORD, std::array<time_t, 3> >::iterator it = s_mapSkillCanUseTime.begin(); it != s_mapSkillCanUseTime.end(); )
		{
			if (it->first != (DWORD) pChar->GetVID() && NULL == CHARACTER_MANAGER::instance().Find(it->first))
				it = s_mapSkillCanUseTime.erase(it);
			else
				++it;
		}
	}

	std::map<DWORD, std::array<time_t, 3> >::iterator it = s_mapSkillCanUseTime.find((DWORD) pChar->GetVID());

	if (it == s_mapSkillCanUseTime.end())
	{
		std::array<time_t, 3> empty;
		empty.fill(0);
		it = s_mapSkillCanUseTime.insert(std::make_pair((DWORD) pChar->GetVID(), empty)).first;
	}

	return it->second;
}

int BlueDragon_StateBattle(LPCHARACTER pChar)
{
	if (pChar->GetHPPct() > 98)
		return PASSES_PER_SEC(1);

	const int SkillCount = 3;
	int SkillPriority[SkillCount];
	// MT2009_PLUS_BLUE_DRAGON_V1 (cooldown use): this dragon's own cooldowns (was: one static array).
	std::array<time_t, 3>& timeSkillCanUseTime = BlueDragon_SkillCanUseTime(pChar);

	if (pChar->GetHPPct() > 76)
	{
		SkillPriority[0] = 1;
		SkillPriority[1] = 0;
		SkillPriority[2] = 2;
	}
	else if (pChar->GetHPPct() > 31)
	{
		SkillPriority[0] = 0;
		SkillPriority[1] = 1;
		SkillPriority[2] = 2;
	}
	else
	{
		SkillPriority[0] = 0;
		SkillPriority[1] = 2;
		SkillPriority[2] = 1;
	}

	time_t timeNow = static_cast<time_t>(get_dword_time());

	for (int i=0 ; i < SkillCount ; ++i)
	{
		const int SkillIndex = SkillPriority[i];

		if (timeSkillCanUseTime[SkillIndex] < timeNow)
		{
			int SkillUsingDuration =
				static_cast<int>(CMotionManager::instance().GetMotionDuration( pChar->GetRaceNum(), MAKE_MOTION_KEY(MOTION_MODE_GENERAL, MOTION_SPECIAL_1 + SkillIndex) ));

			timeSkillCanUseTime[SkillIndex] = timeNow + (UseBlueDragonSkill( pChar, SkillIndex ) * 1000) + SkillUsingDuration + 3000;

			pChar->SendMovePacket(FUNC_MOB_SKILL, SkillIndex, pChar->GetX(), pChar->GetY(), 0, timeNow);

			return 0 == SkillUsingDuration ? PASSES_PER_SEC(1) : PASSES_PER_SEC(SkillUsingDuration);
		}
	}

	return PASSES_PER_SEC(1);
}

int BlueDragon_Damage (LPCHARACTER me, LPCHARACTER pAttacker, int dam)
{
	if (NULL == me || NULL == pAttacker)
		return dam;

	if (true == pAttacker->IsMonster() && 2493 == pAttacker->GetMobTable().dwVnum)
	{
		for (int i=1 ; i <= 4 ; ++i)
		{
			if (ATK_BONUS == BlueDragon_GetIndexFactor("DragonStone", i, "effect_type"))
			{
				DWORD dwDragonStoneID = BlueDragon_GetIndexFactor("DragonStone", i, "vnum");
				size_t val = BlueDragon_GetIndexFactor("DragonStone", i, "val");
				size_t cnt = SECTREE_MANAGER::instance().GetMonsterCountInMap( pAttacker->GetMapIndex(), dwDragonStoneID );

				dam += (dam * (val*cnt))/100;

				break;
			}
		}
	}

	if (true == me->IsMonster() && 2493 == me->GetMobTable().dwVnum)
	{
		for (int i=1 ; i <= 4 ; ++i)
		{
			if (DEF_BONUS == BlueDragon_GetIndexFactor("DragonStone", i, "effect_type"))
			{
				DWORD dwDragonStoneID = BlueDragon_GetIndexFactor("DragonStone", i, "vnum");
				size_t val = BlueDragon_GetIndexFactor("DragonStone", i, "val");
				size_t cnt = SECTREE_MANAGER::instance().GetMonsterCountInMap( me->GetMapIndex(), dwDragonStoneID );

				dam -= (dam * (val*cnt))/100;

				if (dam <= 0)
					dam = 1;

				break;
			}
		}
	}

	if (true == me->IsStone() && 0 != pAttacker->GetMountVnum())
	{
		for (int i=1 ; i <= 4 ; ++i)
		{
			if (me->GetMobTable().dwVnum == BlueDragon_GetIndexFactor("DragonStone", i, "vnum"))
			{
				if (pAttacker->GetMountVnum() == BlueDragon_GetIndexFactor("DragonStone", i, "enemy"))
				{
					size_t val = BlueDragon_GetIndexFactor("DragonStone", i, "enemy_val");

					dam *= val;

					break;
				}
			}
		}
	}

	return dam;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
