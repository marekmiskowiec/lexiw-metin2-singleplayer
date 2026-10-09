#include "stdafx.h"
#include <set>
#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
#include "desc_client.h"
#include "db.h"
#include "log.h"
#include "skill.h"
#include "text_file_loader.h"
#include "priv_manager.h"
#include "questmanager.h"
#include "unique_item.h"
#include "safebox.h"
#include "blend_item.h"
#include "locale_service.h"
#include "item.h"
#include "item_manager.h"

extern int g_iMoonlightChestPermille;
extern int g_iMoonlightChestStonePermille;
extern int g_iDragonCoinStonePermille;
extern int g_iDragonCoinBossPermille;
extern int g_iBlessingScrollStonePermille;
#include "mob_manager.h"

#include "../../common/VnumHelper.h"
#include "DragonSoul.h"
#include "cube.h"

// MT2009_CLASSIC_EDITION_V1 (server-patches/classic): the edition this core runs,
// from M2_EDITION (compose, .env). MT2009 Classic leaves out the costumes,
// hairstyles, weapon skins, mounts, pets (seals, boxes, the new pets and their
// eggs), the alchemy (Dragon Stones, Cor Draconis, shards), the sashes, the Wheel
// ticket, the Battle Pass's pass, the costume reagents and Digi Rasta's
// awakening goods (Kamien Przebudzenia, Olejek Niebios, soul stones +5..+9,
// awakened weapons): no such item is ever made, from a drop, a box, a shop, a
// quest or a game master's /i alike.
bool Mt2009IsClassic()
{
	static int s_iClassic = -1;
	if (s_iClassic < 0)
	{
		const char* e = getenv("M2_EDITION");
		s_iClassic = (e && (0 == strcasecmp(e, "classic"))) ? 1 : 0;
	}
	return s_iClassic == 1;
}

bool Mt2009ClassicBansItem(DWORD vnum, BYTE bType)
{
	if (!Mt2009IsClassic())
		return false;
	switch (bType)
	{
		case ITEM_COSTUME:
		case ITEM_DS:
		case ITEM_SPECIAL_DS:
		case ITEM_PET:
			return true;
	}
	if ((vnum >= 53001 && vnum <= 53999) || (vnum >= 55001 && vnum <= 55999) || vnum == 38200 || vnum == 38201)
		return true;	// pets, pet boxes, new pets and eggs
	if (vnum >= 71114 && vnum <= 71121)
		return true;	// mount seals
	if ((vnum >= 50255 && vnum <= 50259) || vnum == 30270 || vnum == 100300)
		return true;	// Cor Draconis, Dragon Stone shards, dragon beans
	if ((vnum >= 70063 && vnum <= 70065) || vnum == 80030 || vnum == 72199)
		return true;	// costume reagents, Wheel ticket, Battle Pass pass
	if (vnum == 30670 || vnum == 71056)
		return true;	// Kamien Przebudzenia, Olejek Niebios
	if ((vnum >= 28530 && vnum <= 28543) || (vnum >= 28600 && vnum <= 28613) || (vnum >= 28700 && vnum <= 28713)
		|| (vnum >= 28800 && vnum <= 28813) || (vnum >= 28900 && vnum <= 28913))
		return true;	// soul stones +5..+9
	if ((vnum >= 210 && vnum <= 229) || (vnum >= 1160 && vnum <= 1169) || (vnum >= 2190 && vnum <= 2199)
		|| (vnum >= 3170 && vnum <= 3179) || (vnum >= 5150 && vnum <= 5159) || (vnum >= 7170 && vnum <= 7179))
		return true;	// the awakened weapons
	return false;
}

ITEM_MANAGER::ITEM_MANAGER()
	: m_iTopOfTable(0), m_dwVIDCount(0), m_dwCurrentID(0)
{
	m_ItemIDRange.dwMin = m_ItemIDRange.dwMax = m_ItemIDRange.dwUsableItemIDMin = 0;
	m_ItemIDSpareRange.dwMin = m_ItemIDSpareRange.dwMax = m_ItemIDSpareRange.dwUsableItemIDMin = 0;
}

ITEM_MANAGER::~ITEM_MANAGER()
{
	Destroy();
}

void ITEM_MANAGER::Destroy()
{
	itertype(m_VIDMap) it = m_VIDMap.begin();
	for ( ; it != m_VIDMap.end(); ++it) {
#ifdef M2_USE_POOL
		pool_.Destroy(it->second);
#else
		M2_DELETE(it->second);
#endif
	}
	m_VIDMap.clear();
}

void ITEM_MANAGER::GracefulShutdown()
{
	TR1_NS::unordered_set<LPITEM>::iterator it = m_set_pkItemForDelayedSave.begin();

	while (it != m_set_pkItemForDelayedSave.end())
		SaveSingleItem(*(it++));

	m_set_pkItemForDelayedSave.clear();
}

bool ITEM_MANAGER::Initialize(TItemTable * table, int size)
{
	m_vec_prototype.clear();
	m_vec_prototype.resize(size);
	m_map_ItemRefineFrom.clear();
	#ifdef ENABLE_ITEM_PROTO_MAP
	m_mapItemProto.clear();
	#endif

	thecore_memcpy(&m_vec_prototype[0], table, sizeof(TItemTable) * size);

	for (int i = 0; i < size; i++)
	{
		#ifdef ENABLE_ITEM_PROTO_MAP
		m_mapItemProto[m_vec_prototype[i].dwVnum] = &m_vec_prototype[i];
		#endif

		if (0 != m_vec_prototype[i].dwVnumRange)
			m_vec_item_vnum_range_info.emplace_back( &m_vec_prototype[i]);

		if (m_vec_prototype[i].dwRefinedVnum)
			m_map_ItemRefineFrom.emplace(m_vec_prototype[i].dwRefinedVnum, m_vec_prototype[i].dwVnum);

		if (m_vec_prototype[i].bType == ITEM_QUEST || IS_SET(m_vec_prototype[i].dwFlags, ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE)
			#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
			|| (m_vec_prototype[i].bType == ITEM_COSTUME && m_vec_prototype[i].bSubType == COSTUME_MOUNT)
			#endif
		)
			quest::CQuestManager::instance().RegisterNPCVnum(m_vec_prototype[i].dwVnum);

		m_map_vid.emplace(m_vec_prototype[i].dwVnum, m_vec_prototype[i]);
		if (test_server)
			sys_log( 0, "ITEM_INFO %d %s ", m_vec_prototype[i].dwVnum, m_vec_prototype[i].szLocaleName );
	}

	// when type.use begin
	for (BYTE i = ITEM_NONE + 1; i < ITEM_MAX_TYPE_NUM; i++)
		quest::CQuestManager::instance().RegisterNPCVnum(i);

	ITEM_VID_MAP::iterator it = m_VIDMap.begin();
	sys_log (1, "ITEM_VID_MAP %d", m_VIDMap.size() );
	while (it != m_VIDMap.end())
	{
		LPITEM item = it->second;
		++it;

		const TItemTable* tableInfo = GetTable(item->GetOriginalVnum());
		if (!tableInfo)
		{
			sys_err("cannot reset item table for %d", item->GetOriginalVnum());
			item->SetProto(NULL);
			continue;
		}
		item->SetProto(tableInfo);
	}

	return true;
}

LPITEM ITEM_MANAGER::CreateItem(DWORD vnum, ITEM_COUNT count, DWORD id, bool bTryMagic, int iRarePct, bool bSkipSave, int addRarePct)
{
	if (0 == vnum)
		return NULL;

	DWORD dwMaskVnum = 0;
	if (GetMaskVnum(vnum))
		dwMaskVnum = GetMaskVnum(vnum);

	const TItemTable* table = GetTable(vnum);
	if (NULL == table)
		return NULL;

	// MT2009_CLASSIC_EDITION_V1 (create): MT2009 Classic never makes an item it
	// leaves out; one already saved (id != 0) still loads.
	if (0 == id && Mt2009ClassicBansItem(vnum, table->bType))
		return NULL;

	LPITEM item = NULL;

	if (m_map_pkItemByID.find(id) != m_map_pkItemByID.end())
	{
		item = m_map_pkItemByID[id];
		LPCHARACTER owner = item->GetOwner();
		sys_err("ITEM_ID_DUP: %u %s owner %p", id, item->GetName(), get_pointer(owner));
		return NULL;
	}

#ifdef M2_USE_POOL
	item = pool_.Construct();
#else
	item = M2_NEW CItem(vnum);
#endif

	bool bIsNewItem = (0 == id);

	item->Initialize();
	item->SetProto(table);
	item->SetMaskVnum(dwMaskVnum);

	if (item->GetType() == ITEM_ELK)
		item->SetSkipSave(true);
#if defined(ENABLE_CHEQUE_SYSTEM) && !defined(DISABLE_CHEQUE_DROP)
	else if (item->GetVnum() == CHEQUE_VNUM)
		item->SetSkipSave(true);
#endif

	else if (!bIsNewItem)
	{
		item->SetID(id);
		item->SetSkipSave(true);
	}
	else
	{
		item->SetID(GetNewID());

		if (item->GetType() == ITEM_UNIQUE)
		{
			if (item->GetSubType() == UNIQUE_USE_COUNT)
			{
				item->SetSocket(0, item->GetValue(0), true, "SOCKET_UNIQUE");
			}
			else
			{
				if (item->GetValue(2) == 0)
					item->SetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, item->GetValue(0), true, "SOCKET_UNIQUE");
				else
				{
					//int globalTime = get_global_time();
					//int lastTime = item->GetValue(0);
					//int endTime = get_global_time() + item->GetValue(0);
					item->SetSocket(ITEM_SOCKET_UNIQUE_REMAIN_TIME, get_global_time() + item->GetValue(0), true, "SOCKET_UNIQUE");
				}
			}
		}

		if (IsUnique70LevelWeapon(vnum))
		{
			BYTE randomPoint = aiUniqueApplyWeapon70Types[number(0, std::size(aiUniqueApplyWeapon70Types)-1)];
			auto apply = GetUniqueApplyWeapon70(randomPoint, item->GetRefineLevel());
			if (apply.bType != POINT_NONE)
				item->SetForceAttribute(5, randomPoint, apply.lValue); // ustawiamy jako 6 bonus
		}
	}

	switch (item->GetVnum())
	{
		case ITEM_AUTO_HP_RECOVERY_S:
		case ITEM_AUTO_HP_RECOVERY_M:
		case ITEM_AUTO_HP_RECOVERY_L:
		case ITEM_AUTO_HP_RECOVERY_X:
		case ITEM_AUTO_SP_RECOVERY_S:
		case ITEM_AUTO_SP_RECOVERY_M:
		case ITEM_AUTO_SP_RECOVERY_L:
		case ITEM_AUTO_SP_RECOVERY_X:
		case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
		case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
		case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
		case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
		case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
		case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
			// Socket 1 is the capacity used, socket 2 the capacity there is
			// (AutoRecoveryItemProcess, idx_of_amount_of_used / _full), and the
			// use path refuses an elixir whose two are equal as empty. The
			// package set both to the full capacity here, so every elixir this
			// engine ever created was born used up; a loaded one gets its saved
			// sockets back a moment later and never noticed. A new one starts
			// with nothing used (2.0.13).
			item->SetSocket(1, bIsNewItem ? 0 : item->GetValue(0), false);
			item->SetSocket(2, item->GetValue(0), bIsNewItem);
			break;
	}

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	if ((item->GetType() == ITEM_COSTUME) && (item->GetSubType() == COSTUME_ACCE) && (item->GetSocket(ACCE_ABSORPTION_SOCKET) == 0))
	{
		auto lVal = item->GetValue(ACCE_GRADE_VALUE_FIELD);
		switch (lVal)
		{
		case 2:
			lVal = ACCE_GRADE_2_ABS;
			break;
		case 3:
			lVal = ACCE_GRADE_3_ABS;
			break;
		case 4:
			lVal = number(ACCE_GRADE_4_ABS_MIN, ACCE_GRADE_4_ABS_MAX_COMB);
			break;
		default:
			lVal = ACCE_GRADE_1_ABS;
			break;
		}
		item->SetSocket(ACCE_ABSORPTION_SOCKET, lVal);
	}
#endif

	if (item->GetType() == ITEM_ELK)
		;
#if defined(ENABLE_CHEQUE_SYSTEM) && !defined(DISABLE_CHEQUE_DROP)
	else if (item->GetVnum() == CHEQUE_VNUM)
		;
#endif
	else if (item->IsStackable())
	{
		count = MINMAX(1, count, item->GetMaxStack());

		if (bTryMagic && count <= 1 && IS_SET(item->GetFlag(), ITEM_FLAG_MAKECOUNT))
			count = item->GetValue(1);
	}
	else
		count = 1;

	item->SetVID(++m_dwVIDCount);

	if (bSkipSave == false)
		m_VIDMap.emplace(item->GetVID(), item);

	if (item->GetID() != 0 && bSkipSave == false)
		m_map_pkItemByID.emplace(item->GetID(), item);

	if (!item->SetCount(count))
		return NULL;

	item->SetSkipSave(false);

	if (item->GetType() == ITEM_UNIQUE && item->GetValue(2) != 0)
		item->StartUniqueExpireEvent();

	for (int i=0 ; i < ITEM_LIMIT_MAX_NUM ; i++)
	{
		if (LIMIT_REAL_TIME == item->GetLimitType(i))
		{
			if (item->GetLimitValue(i))
			{
				item->SetSocket(0, time(0) + item->GetLimitValue(i));
			}
			else
			{
				item->SetSocket(0, time(0) + 60*60*24*7);
			}

			item->StartRealTimeExpireEvent();
		}

		else if (LIMIT_TIMER_BASED_ON_WEAR == item->GetLimitType(i))
		{
			if (true == item->IsEquipped())
			{
				item->StartTimerBasedOnWearExpireEvent();
			}
			else if(0 == id)
			{
				long duration = item->GetSocket(0);
				if (0 == duration)
					duration = item->GetLimitValue(i);

				if (0 == duration)
					duration = 60 * 60 * 10;

				item->SetSocket(0, duration);
			}
		}
	}

	if (id == 0)
	{
		if (ITEM_BLEND==item->GetType())
		{
			if (Blend_Item_find(item->GetVnum()))
			{
				Blend_Item_set_value(item);
				return item;
			}
		}

		if (table->sAddonType)
		{
			item->ApplyAddon(table->sAddonType);
		}

		if (bTryMagic)
		{
			int base_rare_pct = table->bAlterToMagicItemPct;
			if (iRarePct == -1 || iRarePct > 0 && iRarePct < base_rare_pct)
				iRarePct = base_rare_pct;

			int real_chance = iRarePct > 0 ? (iRarePct + addRarePct) : 0;

			if (number(1, 100) <= real_chance)
				item->AlterToMagicItem();
		}

		if (table->bGainSocketPct)
			item->AlterToSocketItem(table->bGainSocketPct);

		if (vnum == 50300 || vnum == ITEM_SKILLFORGET_VNUM)
		{
			extern const DWORD GetRandomSkillVnum(BYTE bJob = JOB_MAX_NUM);
			item->SetSocket(0, GetRandomSkillVnum());
		}
		else if (ITEM_SKILLFORGET2_VNUM == vnum)
		{
			DWORD dwSkillVnum;

			do
			{
				dwSkillVnum = number(112, 119);

				if (NULL != CSkillManager::instance().Get(dwSkillVnum))
					break;
			} while (true);

			item->SetSocket(0, dwSkillVnum);
		}
	}
	else
	{
		if (100 == table->bAlterToMagicItemPct && 0 == item->GetAttributeCount())
		{
			item->AlterToMagicItem();
		}
	}

	if (item->GetType() == ITEM_QUEST)
	{
		for (itertype (m_map_pkQuestItemGroup) it = m_map_pkQuestItemGroup.begin(); it != m_map_pkQuestItemGroup.end(); it++)
		{
			if (it->second->m_bType == CSpecialItemGroup::QUEST && it->second->Contains(vnum))
			{
				item->SetSIGVnum(it->first);
			}
		}
	}
	else if (item->GetType() == ITEM_UNIQUE)
	{
		for (itertype (m_map_pkSpecialItemGroup) it = m_map_pkSpecialItemGroup.begin(); it != m_map_pkSpecialItemGroup.end(); it++)
		{
			if (it->second->m_bType == CSpecialItemGroup::SPECIAL && it->second->Contains(vnum))
			{
				item->SetSIGVnum(it->first);
			}
		}
	}

	if (item->IsDragonSoul() && 0 == id)
	{
		DSManager::instance().DragonSoulItemInitialize(item);
	}
	else if (item->IsDragonSoul() && 0 != id)
	{
		// One-time repair for items saved before the Apply_Type parsing fix
		// (17 September 2026) -- see DSManager::RepairZeroAttributeItem().
		// A no-op for any item that already has a real attribute.
		DSManager::instance().RepairZeroAttributeItem(item);
	}
	return item;
}

void ITEM_MANAGER::DelayedSave(LPITEM item)
{
	if (item->GetID() != 0)
		m_set_pkItemForDelayedSave.emplace(item);
}

void ITEM_MANAGER::FlushDelayedSave(LPITEM item)
{
	TR1_NS::unordered_set<LPITEM>::iterator it = m_set_pkItemForDelayedSave.find(item);

	if (it == m_set_pkItemForDelayedSave.end())
	{
		return;
	}

	m_set_pkItemForDelayedSave.erase(it);
	SaveSingleItem(item);
}

void ITEM_MANAGER::SaveSingleItem(LPITEM item)
{
	if (!item->GetOwner())
	{
		DWORD dwID = item->GetID();
		DWORD dwOwnerID = item->GetLastOwnerPID();

		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_DESTROY, 0, sizeof(DWORD) + sizeof(DWORD));
		db_clientdesc->Packet(&dwID, sizeof(DWORD));
		db_clientdesc->Packet(&dwOwnerID, sizeof(DWORD));

		sys_log(1, "ITEM_DELETE %s:%u", item->GetName(), dwID);
		return;
	}

	sys_log(1, "ITEM_SAVE %s:%d in %s window %d", item->GetName(), item->GetID(), item->GetOwner()->GetName(), item->GetWindow());

	TPlayerItem t;

	t.id = item->GetID();
	t.window = item->GetWindow();
	switch (t.window)
	{
		case EQUIPMENT:
			t.pos = item->GetCell() - INVENTORY_MAX_NUM;
			break;
#ifdef ENABLE_BELT_INVENTORY_EX
		case INVENTORY:
			if (BELT_INVENTORY_SLOT_START <= item->GetCell() && BELT_INVENTORY_SLOT_END > item->GetCell())
			{
				t.window = BELT_INVENTORY;
				t.pos = item->GetCell() - BELT_INVENTORY_SLOT_START;
				break;
			}
#endif
		default:
			t.pos = item->GetCell();
			break;
	}
	t.count = item->GetCount();
	t.vnum = item->GetOriginalVnum();
	switch (t.window)
	{
		case SAFEBOX:
		case MALL:
			t.owner = item->GetOwner()->GetDesc()->GetAccountTable().id;
			break;
		default:
			t.owner = item->GetOwner()->GetPlayerID();
			break;
	}
	thecore_memcpy(t.alSockets, item->GetSockets(), sizeof(t.alSockets));
	thecore_memcpy(t.aAttr, item->GetAttributes(), sizeof(t.aAttr));

	db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_SAVE, 0, sizeof(TPlayerItem));
	db_clientdesc->Packet(&t, sizeof(TPlayerItem));
}

void ITEM_MANAGER::RemoveFromDelayedSave(LPITEM item)
{
	if (!item)
		return;

	auto it = m_set_pkItemForDelayedSave.find(item);
	if (it != m_set_pkItemForDelayedSave.end())
		m_set_pkItemForDelayedSave.erase(it);
}

void ITEM_MANAGER::Update()
{
	TR1_NS::unordered_set<LPITEM>::iterator it = m_set_pkItemForDelayedSave.begin();
	TR1_NS::unordered_set<LPITEM>::iterator this_it;

	while (it != m_set_pkItemForDelayedSave.end())
	{
		this_it = it++;
		LPITEM item = *this_it;

		SaveSingleItem(item);

		m_set_pkItemForDelayedSave.erase(this_it);
	}
}

void ITEM_MANAGER::RemoveItem(LPITEM item, const char * c_pszReason)
{
	LPCHARACTER o;

	if ((o = item->GetOwner()))
	{
		char szHint[64 + 1];
		snprintf(szHint, sizeof(szHint), "%s %d", item->GetName(), item->GetCount());
		LogManager::instance().ItemLog(o, item, c_pszReason ? c_pszReason : "REMOVE", szHint, true);

		// SAFEBOX_TIME_LIMIT_ITEM_BUG_FIX
		if (item->GetWindow() == MALL || item->GetWindow() == SAFEBOX)
		{
			CSafebox* pSafebox = item->GetWindow() == MALL ? o->GetMall() : o->GetSafebox();
			if (pSafebox)
			{
				pSafebox->Remove(item->GetCell());
			}
		}
		// END_OF_SAFEBOX_TIME_LIMIT_ITEM_BUG_FIX
		else
		{
			o->SyncQuickslot(QUICKSLOT_TYPE_ITEM, item->GetCell(), 255);
			item->RemoveFromCharacter();
		}
	}

	M2_DESTROY_ITEM(item);
}

#ifndef DEBUG_ALLOC
void ITEM_MANAGER::DestroyItem(LPITEM item)
#else
void ITEM_MANAGER::DestroyItem(LPITEM item, const char* file, size_t line)
#endif
{
	if (item->GetSectree())
		item->RemoveFromGround();

	if (item->GetOwner())
	{
		if (CHARACTER_MANAGER::instance().Find(item->GetOwner()->GetPlayerID()) != NULL)
		{
			sys_err("DestroyItem: GetOwner %s %s!!", item->GetName(), item->GetOwner()->GetName());
			item->RemoveFromCharacter();
		}
		else
		{
			sys_err ("WTH! Invalid item owner. owner pointer : %p", item->GetOwner());
		}
	}

	TR1_NS::unordered_set<LPITEM>::iterator it = m_set_pkItemForDelayedSave.find(item);

	if (it != m_set_pkItemForDelayedSave.end())
		m_set_pkItemForDelayedSave.erase(it);

	DWORD dwID = item->GetID();
	sys_log(2, "ITEM_DESTROY %s:%u", item->GetName(), dwID);

	if (!item->GetSkipSave() && dwID)
	{
		DWORD dwOwnerID = item->GetLastOwnerPID();

		db_clientdesc->DBPacketHeader(HEADER_GD_ITEM_DESTROY, 0, sizeof(DWORD) + sizeof(DWORD));
		db_clientdesc->Packet(&dwID, sizeof(DWORD));
		db_clientdesc->Packet(&dwOwnerID, sizeof(DWORD));
	}
	else
	{
		sys_log(2, "ITEM_DESTROY_SKIP %s:%u (skip=%d)", item->GetName(), dwID, item->GetSkipSave());
	}

	if (dwID)
		m_map_pkItemByID.erase(dwID);

	m_VIDMap.erase(item->GetVID());

#ifdef M2_USE_POOL
	pool_.Destroy(item);
#else
#ifndef DEBUG_ALLOC
	M2_DELETE(item);
#else
	M2_DELETE_EX(item, file, line);
#endif
#endif
}

LPITEM ITEM_MANAGER::Find(DWORD id)
{
	itertype(m_map_pkItemByID) it = m_map_pkItemByID.find(id);
	if (it == m_map_pkItemByID.end())
		return NULL;
	return it->second;
}

LPITEM ITEM_MANAGER::FindByVID(DWORD vid)
{
	ITEM_VID_MAP::iterator it = m_VIDMap.find(vid);

	if (it == m_VIDMap.end())
		return NULL;

	return (it->second);
}

TItemTable * ITEM_MANAGER::GetTable(DWORD vnum)
{
	#ifdef ENABLE_ITEM_PROTO_MAP
	if (const auto it = m_mapItemProto.find(vnum); it != m_mapItemProto.end())
		return it->second;
	// dragon soul vnum range
	else if (const auto it = m_mapItemProto.find(vnum - (vnum % 100));
			it != m_mapItemProto.end()
			&& it->second->dwVnumRange
			&& it->second->dwVnum <= vnum && vnum <= it->second->dwVnum + it->second->dwVnumRange)
		return it->second;
	return nullptr;
	#else
	int rnum = RealNumber(vnum);
	if (rnum < 0) // dragon soul vnum range search
	{
		for (size_t i = 0; i < m_vec_item_vnum_range_info.size(); i++)
		{
			TItemTable* p = m_vec_item_vnum_range_info[i];
			if ((p->dwVnum < vnum) &&
				vnum < (p->dwVnum + p->dwVnumRange))
			{
				return p;
			}
		}

		return NULL;
	}

	return &m_vec_prototype[rnum];
	#endif
}

int ITEM_MANAGER::RealNumber(DWORD vnum)
{
	int bot, top, mid;

	bot = 0;
	top = m_vec_prototype.size();

	TItemTable * pTable = &m_vec_prototype[0];
	while (1) // primitive binary tree search
	{
		mid = (bot + top) >> 1;

		if ((pTable + mid)->dwVnum == vnum)
			return (mid);

		if (bot >= top)
			return (-1);

		if ((pTable + mid)->dwVnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}
}

bool ITEM_MANAGER::GetVnum(const char * c_pszName, DWORD & r_dwVnum)
{
	int len = strlen(c_pszName);

	TItemTable * pTable = &m_vec_prototype[0];

	for (DWORD i = 0; i < m_vec_prototype.size(); ++i, ++pTable)
	{
		if (!strncasecmp(c_pszName, pTable->szLocaleName, len))
		{
			r_dwVnum = pTable->dwVnum;
			return true;
		}
	}

	return false;
}

bool ITEM_MANAGER::GetVnumByOriginalName(const char * c_pszName, DWORD & r_dwVnum)
{
	//int len = strlen(c_pszName);

	//TItemTable * pTable = &m_vec_prototype[0];

	//for (DWORD i = 0; i < m_vec_prototype.size(); ++i, ++pTable)
	//{
	//	if (!strncasecmp(c_pszName, pTable->szName, len))
	//	{
	//		r_dwVnum = pTable->dwVnum;
	//		return true;
	//	}
	//}

	return false;
}

class CItemDropInfo
{
	public:
		CItemDropInfo(int iLevelStart, int iLevelEnd, int iPercent, DWORD dwVnum) :
			m_iLevelStart(iLevelStart), m_iLevelEnd(iLevelEnd), m_iPercent(iPercent), m_dwVnum(dwVnum)
			{
			}

		int	m_iLevelStart;
		int	m_iLevelEnd;
		int	m_iPercent; // 1 ~ 1000
		DWORD	m_dwVnum;

		friend bool operator < (const CItemDropInfo & l, const CItemDropInfo & r)
		{
			return l.m_iLevelEnd < r.m_iLevelEnd;
		}
};

extern std::vector<CItemDropInfo> g_vec_pkCommonDropItem[MOB_RANK_MAX_NUM];

// 20050503.ipkn.

int GetDropPerKillPct(int iMinimum, int iDefault, int iDeltaPercent, const char * c_pszFlag)
{
	int iVal = 0;

	if ((iVal = quest::CQuestManager::instance().GetEventFlag(c_pszFlag)))
	{
		if (!test_server)
		{
			if (iVal < iMinimum)
				iVal = iDefault;

			if (iVal < 0)
				iVal = iDefault;
		}
	}

	if (iVal == 0)
		return 0;

	return (40000 * iDeltaPercent / iVal);
}

void AddToBonusItemDrop(int& itemDropBonus, int value)
{
	//if (itemDropBonus >= 100)
	//{
	//	itemDropBonus += value * 75 / 100;
	//}
	//else
	//{
	//	itemDropBonus += value;
	//}
	itemDropBonus += value;
}

bool ITEM_MANAGER::GetDropPct(LPCHARACTER pkChr, LPCHARACTER pkKiller, OUT int& iDeltaPercent, OUT int& iRandRange)
{
	if (pkChr == NULL || pkKiller == NULL)
		return false;

	return GetDropPct(pkKiller, pkChr->GetRaceNum(), pkChr->GetMobRank(), pkChr->GetLevel(), pkChr->IsStone(), iDeltaPercent, iRandRange);
}

bool ITEM_MANAGER::GetDropPct(LPCHARACTER pkKiller, DWORD race, BYTE bMobRank, int iMobLevel, bool IsStone, OUT int& iDeltaPercent, OUT int& iRandRange)
{
	if (NULL == pkKiller)
		return false;

	int iLevel = pkKiller->GetLevel();
	iDeltaPercent = 100;

	
	bool hasThiefGloves = pkKiller->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM);
	if (IsStone || bMobRank >= MOB_RANK_BOSS)
	{
		iDeltaPercent = PERCENT_LVDELTA_BOSS(pkKiller->GetLevel(), iMobLevel);
	}
	else
	{
		if (!IsMiniBoss(race) && pkKiller->IsEquipUniqueItem(UNIQUE_ITEM_DOUBLE_ITEM_BOSS_METIN_ONLY))
			hasThiefGloves = false;

		iDeltaPercent = PERCENT_LVDELTA(pkKiller->GetLevel(), iMobLevel);
	}

	sys_log(3, "CreateDropItem for level: %d rank: %u pct: %d", iLevel, bMobRank, iDeltaPercent);
	iDeltaPercent = iDeltaPercent * CHARACTER_MANAGER::instance().GetMobItemRate(pkKiller) / 100;

	int item_drop_bonus = 0;
	if (pkKiller->GetPremiumRemainSeconds(PREMIUM_ITEM) > 0)
		AddToBonusItemDrop(item_drop_bonus, 100);

	if (hasThiefGloves)
		AddToBonusItemDrop(item_drop_bonus, 100);

	int addDropEventFlag = quest::CQuestManager::instance().GetEventFlag("big_drop");
	if (addDropEventFlag > 0)
	{
		AddToBonusItemDrop(item_drop_bonus, addDropEventFlag);
	}

	int priv_item_bonus = CPrivManager::instance().GetPriv(pkKiller, PRIV_ITEM_DROP);
	if (priv_item_bonus > 0)
		AddToBonusItemDrop(item_drop_bonus, priv_item_bonus);

	//iDeltaPercent = iDeltaPercent * (100 + MIN(item_drop_bonus, 225)) / 100;
	iDeltaPercent = iDeltaPercent * (100 + item_drop_bonus) / 100;

	iRandRange = 4000000;
	if (distribution_test_server) iRandRange /= 3;

	return true;
}

void SetDropRarePct(LPCHARACTER pkMob, int& rarePct, TItemTable * itemTable)
{
	if (pkMob == NULL)
		return;

	if (itemTable == NULL)
		return;

	int setPct = 0;
	if (pkMob->IsStone())
	{
		setPct = MAX(rarePct, 30);
	}
	else if (pkMob->GetMobRank() >= MOB_RANK_BOSS && !pkMob->IsStone()) {
		setPct = MAX(rarePct, 100);
	}

	DWORD dwRace = pkMob->GetRaceNum();
	if (dwRace >= 191 && dwRace <= 194)
	{
		setPct = MAX(rarePct, 25);
	}
	else if (dwRace >= 491 && dwRace <= 494 ||	// m2
		dwRace == 681 ||                    // dolina
		dwRace == 781 || dwRace == 782 ||   // hwang
		dwRace == 1181 || dwRace == 1182 || // sohan
		dwRace == 2181 || dwRace == 2182 || // pustynia
		dwRace == 2281 || dwRace == 2282 || // pieklo
		dwRace == 2381 || dwRace == 2382)   // las
	{
		setPct = MAX(rarePct, 100);
	}

	if (itemTable->bType == ITEM_ARMOR)
	{
		if (itemTable->bSubType == ARMOR_BODY || itemTable->bSubType == ARMOR_HEAD || itemTable->bSubType == ARMOR_SHIELD)
			setPct = MAX(rarePct, setPct * 80 / 100);
		else
		{
			setPct = MAX(rarePct, setPct * 50 / 100);
		}
	}

	rarePct = setPct;
}

// How far above a Metin stone a killer may be and still get the stone's
// guaranteed skill book (the top-up in CreateDropItem below).
static const int PLAYERBOT_METIN_BOOK_LEVEL_DELTA = 15;
// The stones a Blessing Scroll may come from (Iwakura, 24 September).
static const int PLAYERBOT_BLESSING_SCROLL_STONE_MIN_LEVEL = 15;
static const int PLAYERBOT_BLESSING_SCROLL_STONE_MAX_LEVEL = 99;

// MT2009_PLUS_DUNGEON_DROP_V1 (boss): the Razador (351) and Nemere (352) dungeons (the owner,
// 29 September). Their bosses, Razador (6091) and Nemere (6191), drop only yang
// (RewardGold, char_battle.cpp) - the loot is the 2-5 chests per player the dungeon
// quests hand out. Their monsters, Metins and mini-bosses (6001-6199, 8057, 8058)
// and whatever else dies on those maps or their instances keep their own tables but
// get none of the world's extras: no skill-book top-up, Moonlight chest, Dragon Coin
// voucher, Cor Draconis, sash, Blessing Scroll, horse book nor the double-loot event.
// MT2009_PLUS_RARE_MOB_RULES_V1 (server-patches/raremobrules): the engine's Cor Draconis and +0 sash
// rolls per mob (the owner, 1 October): a mob listed here rolls its own Cor count and
// chance and its own sash chance (0 = none); map 0 = any map, else the map (or its
// instances, map * 10000 ...) it must die on. Every other mob keeps the usual rules.
struct Mt2009PlusRareMobRule
{
	DWORD race;
	long map;
	int corCount;
	int corChance;
	int sashChance;
};

static long Mt2009PlusBaseMap(LPCHARACTER victim)
{
	const long mapIndex = victim->GetMapIndex();
	return mapIndex >= 10000 ? mapIndex / 10000 : mapIndex;
}

// MT2009_PLUS_DROP_WIKI_V1: the rules by race and base map - map -1 takes only
// the rules for any map (the drop wiki, which has no map).
static const Mt2009PlusRareMobRule* Mt2009PlusRareMobRuleForRace(DWORD race, long map)
{
	static const Mt2009PlusRareMobRule s_aRules[] =
	{
		{ 8006, 363, 0, 0, 0 },	// Metin Ciemnosci in Biblioteka Wiedzy
		{ 9701, 0, 0, 0, 0 },	// Kamien Wzgorza
		{ 9702, 0, 0, 0, 0 },	// Jajo Feniksa
		{ 9684, 0, 0, 0, 0 },	// Plomienny Feniks
		{ 9682, 0, 5, 15, 15 },	// WuKong: 5 Cor at 15%, a sash at 15%
	};
	for (size_t i = 0; i < _countof(s_aRules); ++i)
		if (s_aRules[i].race == race && (s_aRules[i].map == 0 || s_aRules[i].map == map))
			return &s_aRules[i];
	return NULL;
}

static const Mt2009PlusRareMobRule* Mt2009PlusRareMobRuleFor(LPCHARACTER victim)
{
	return Mt2009PlusRareMobRuleForRace(victim->GetRaceNum(), Mt2009PlusBaseMap(victim));
}

// MT2009_PLUS_RARE_MOB_RULES_V1 (drop group): Metin Ciemnosci inside Biblioteka Wiedzy takes the drop
// group of the pseudo mob 98006 (no such monster; mob_drop_item only keys on the number).
static DWORD Mt2009PlusDropGroupRace(LPCHARACTER victim)
{
	if (victim->GetRaceNum() == 8006 && victim->GetMapIndex() >= 10000 && Mt2009PlusBaseMap(victim) == 363)
		return 98006;
	return victim->GetRaceNum();
}

static bool Mt2009PlusDungeonMob(DWORD race)
{
	return (race >= 6001 && race <= 6199) || race == 8057 || race == 8058;
}

static bool Mt2009PlusDungeonPlainDrop(LPCHARACTER victim)
{
	const long mapIndex = victim->GetMapIndex();
	const long base = mapIndex >= 10000 ? mapIndex / 10000 : mapIndex;
	return base == 351 || base == 352 || Mt2009PlusDungeonMob(victim->GetRaceNum());
}

bool ITEM_MANAGER::CreateDropItem(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<LPITEM> & vec_item)
{
	int iDeltaPercent, iRandRange;
	if (pkChr->GetRaceNum() == 6091 || pkChr->GetRaceNum() == 6191)
		return false;
	if (!GetDropPct(pkChr, pkKiller, iDeltaPercent, iRandRange))
		return false;

	int iAddRarePct = pkKiller->GetPoint(POINT_DROP_RARE);

	int iLevel = pkKiller->GetLevel();
	BYTE bRank = pkChr->GetMobRank();
	LPITEM item = NULL;

	// Common Drop Items
	std::vector<CItemDropInfo>::iterator it = g_vec_pkCommonDropItem[bRank].begin();

	while (it != g_vec_pkCommonDropItem[bRank].end())
	{
		const CItemDropInfo & c_rInfo = *(it++);

		if (iLevel < c_rInfo.m_iLevelStart || iLevel > c_rInfo.m_iLevelEnd)
			continue;

		int iPercent = (c_rInfo.m_iPercent * iDeltaPercent) / 100;
		sys_log(3, "CreateDropItem %d ~ %d %d(%d)", c_rInfo.m_iLevelStart, c_rInfo.m_iLevelEnd, c_rInfo.m_dwVnum, iPercent, c_rInfo.m_iPercent);

		if (iPercent >= number(1, iRandRange))
		{
			TItemTable * table = GetTable(c_rInfo.m_dwVnum);

			if (!table)
				continue;

			item = NULL;

			if (table->bType == ITEM_POLYMORPH)
			{
				if (c_rInfo.m_dwVnum == pkChr->GetPolymorphItemVnum())
				{
					item = CreateItem(c_rInfo.m_dwVnum, 1, 0, true);

					if (item)
						item->SetSocket(0, pkChr->GetRaceNum());
				}
			}
			else
			{
				int iRarePct = table->bAlterToMagicItemPct;
				SetDropRarePct(pkChr, iRarePct, table);
				item = CreateItem(c_rInfo.m_dwVnum, 1, 0, true, iRarePct, false, iAddRarePct);
			}

			if (item) vec_item.emplace_back(item);
		}
	}

	// Drop Item Group
	{
		itertype(m_map_pkDropItemGroup) it;
		// MT2009_PLUS_RARE_MOB_RULES_V1 (drop group lookup)
		it = m_map_pkDropItemGroup.find(Mt2009PlusDropGroupRace(pkChr));

		if (it != m_map_pkDropItemGroup.end())
		{
			typeof(it->second->GetVector()) v = it->second->GetVector();

			for (DWORD i = 0; i < v.size(); ++i)
			{
				auto info = v[i];
				int iPercent = (v[i].dwPct * iDeltaPercent) / 100;

				if (iPercent >= number(1, iRandRange))
				{


					DWORD itemVnum = info.dwVnum;
					DWORD itemCount = info.iCount;
					int iRarePct = -1;

					if (info.bSpecialGroup)
					{
						const auto dropInfo = GetSpecialItemGroupDropInfo(info.dwVnum);
						if (dropInfo.dwVnum == 0 || dropInfo.dwCount == 0)
							continue;

						itemVnum = dropInfo.dwVnum;
						itemCount = dropInfo.dwCount;
					}

					TItemTable* table = GetTable(itemVnum);
					SetDropRarePct(pkChr, iRarePct, table);

					item = CreateItem(itemVnum, itemCount, 0, true, iRarePct, false, iAddRarePct);

					if (item)
					{
						if (item->GetType() == ITEM_POLYMORPH)
						{
							if (item->GetVnum() == pkChr->GetPolymorphItemVnum())
							{
								item->SetSocket(0, pkChr->GetRaceNum());
							}
						}

						vec_item.emplace_back(item);
					}
				}
			}
		}
	}

	// MobDropItem Group
	{
		itertype(m_map_pkMobItemGroup) it;
		it = m_map_pkMobItemGroup.find(pkChr->GetRaceNum());

		if ( it != m_map_pkMobItemGroup.end() )
		{
			CMobItemGroup* pGroup = it->second;

			// MOB_DROP_ITEM_BUG_FIX

			if (pGroup && !pGroup->IsEmpty())
			{
				int iPercent = 40000 * iDeltaPercent / pGroup->GetKillPerDrop();
				if (iPercent >= number(1, iRandRange))
				{
					const CMobItemGroup::SMobItemGroupInfo& info = pGroup->GetOne();
					int iRarePct = info.iRarePct;

					TItemTable* table = GetTable(info.dwItemVnum);
					SetDropRarePct(pkChr, iRarePct, table);
					item = CreateItem(info.dwItemVnum, info.iCount, 0, true, iRarePct, false, iAddRarePct);

					if (item) vec_item.emplace_back(item);
				}
			}
			// END_OF_MOB_DROP_ITEM_BUG_FIX
		}
	}

	// Level Item Group
	{
		itertype(m_map_pkLevelItemGroup) it;
		it = m_map_pkLevelItemGroup.find(pkChr->GetRaceNum());

		if ( it != m_map_pkLevelItemGroup.end() )
		{
			if ( it->second->GetLevelLimit() <= (DWORD)iLevel )
			{
				typeof(it->second->GetVector()) v = it->second->GetVector();

				for ( DWORD i=0; i < v.size(); i++ )
				{
					if ( v[i].dwPct >= (DWORD)number(1, 1000000/*iRandRange*/) )
					{
						DWORD dwVnum = v[i].dwVNum;
						item = CreateItem(dwVnum, v[i].iCount, 0, true);
						if ( item ) vec_item.emplace_back(item);
					}
				}
			}
		}
	}

	// BuyerTheitGloves Item Group
	{
		if (pkKiller->GetPremiumRemainSeconds(PREMIUM_ITEM) > 0 ||
				pkKiller->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM))
		{
			itertype(m_map_pkGloveItemGroup) it;
			it = m_map_pkGloveItemGroup.find(pkChr->GetRaceNum());

			if (it != m_map_pkGloveItemGroup.end())
			{
				typeof(it->second->GetVector()) v = it->second->GetVector();

				for (DWORD i = 0; i < v.size(); ++i)
				{
					int iPercent = (v[i].dwPct * iDeltaPercent) / 100;

					if (iPercent >= number(1, iRandRange))
					{
						DWORD dwVnum = v[i].dwVnum;

						int iRarePct = -1;
						TItemTable* table = GetTable(dwVnum);
						SetDropRarePct(pkChr, iRarePct, table);

						item = CreateItem(dwVnum, v[i].iCount, 0, true, iRarePct, false, iAddRarePct);
						if (item) vec_item.emplace_back(item);
					}
				}
			}
		}
	}

	if (pkChr->GetMobDropItemVnum())
	{
		itertype(m_map_dwEtcItemDropProb) it = m_map_dwEtcItemDropProb.find(pkChr->GetMobDropItemVnum());

		if (it != m_map_dwEtcItemDropProb.end())
		{
			int iPercent = (it->second * iDeltaPercent) / 100;

			if (iPercent >= number(1, iRandRange))
			{
				item = CreateItem(pkChr->GetMobDropItemVnum(), 1, 0, true);
				if (item) vec_item.emplace_back(item);
			}
		}
	}

	if (pkChr->IsStone())
	{
		if (pkChr->GetDropMetinStoneVnum())
		{
			int iPercent = (pkChr->GetDropMetinStonePct() * iDeltaPercent) * 400;

			if (iPercent >= number(1, iRandRange))
			{
				item = CreateItem(pkChr->GetDropMetinStoneVnum(), 1, 0, true);
				if (item) vec_item.emplace_back(item);
			}
		}
	}

	// MT2009_PLUS_DUNGEON_DROP_V1 (specials): nothing of the world's extras below in the Razador and
	// Nemere dungeons (Mt2009PlusDungeonPlainDrop above); the block closes before the
	// quest drops.
	if (!Mt2009PlusDungeonPlainDrop(pkChr))
	{
	// One skill book from every Metin stone, whatever its table rolled - the
	// table gives one at a quarter to a full chance, and a stone is where a
	// character learns from, so the count is topped up to one rather than
	// added to. Each book takes its skill the way the table's own does. And
	// not from a stone the killer has outgrown: the engine's own tables
	// fade a drop out by level difference (aiPercentByDeltaLev), this
	// top-up ignored it, and a player of forty-six farmed level-five stones
	// for a guaranteed book each ("Drop z metinow", 12 September). Fifteen
	// levels over the stone is where the top-up ends.
	if (pkChr->IsStone() && pkKiller &&
			pkKiller->GetLevel() <= pkChr->GetLevel() + PLAYERBOT_METIN_BOOK_LEVEL_DELTA)
	{
		int books = 0;
		for (size_t i = 0; i < vec_item.size(); ++i)
			if (vec_item[i] && vec_item[i]->GetVnum() == 50300)
				++books;
		for (; books < 1; ++books)
		{
			item = CreateItem(50300, 1, 0, true);
			if (item) vec_item.emplace_back(item);
		}
	}

	// The Moonlight Treasure Chest event. What a chest holds is decided by
	// special_item_group.txt; how often one appears is decided here, from
	// CONFIG, per kill and per stone.
	{
		const int chestPermille = pkChr->IsStone()
				? g_iMoonlightChestStonePermille : g_iMoonlightChestPermille;
		if (chestPermille > 0 && number(1, 1000) <= chestPermille)
		{
			item = CreateItem(50011, 1, 0, true);
			if (item) vec_item.emplace_back(item);
		}
		// The tables' own chests obey the same zero: a chest window that is
		// shut, or the switch off, means no Moonlight chest from anybody.
		if (g_iMoonlightChestPermille <= 0 && g_iMoonlightChestStonePermille <= 0)
		{
			for (size_t i = 0; i < vec_item.size();)
			{
				if (vec_item[i] && vec_item[i]->GetVnum() == 50011)
				{
					M2_DESTROY_ITEM(vec_item[i]);
					vec_item.erase(vec_item.begin() + i);
				}
				else
					++i;
			}
		}
	}

	// A Dragon Coin voucher (Kupon SM 50, vnum 80017) from a Metin stone
	// or a boss, so the ItemShop currency has a way into the game. Off (0)
	// by default; the permille is CONFIG, per stone and per boss, small on
	// purpose ("niech tych kuponow za duzo nie dropi").
	{
		const int coinPermille = pkChr->IsStone()
				? g_iDragonCoinStonePermille
				: (pkChr->GetMobRank() >= MOB_RANK_BOSS ? g_iDragonCoinBossPermille : 0);
		if (coinPermille > 0 && number(1, 1000) <= coinPermille)
		{
			item = CreateItem(80017, 1, 0, true);
			if (item) vec_item.emplace_back(item);
		}
	}

	// Cor Draconis (Dragon Soul reward box, vnum 50255) drop -- operator's
	// explicit request, 17 September 2026. Deliberately its own standalone
	// block, not routed through GetDropPct()/iDeltaPercent/iRandRange (the
	// level-delta/premium/glove/event/party machinery every other block
	// above uses) -- the chance must be flat and independent of all of that.
	// A real playerbot is still IsPC(), so IsPC() alone is not enough to
	// exclude it; GetDesc()->IsBot() is the actual test (playerbot_manager's
	// own cross-core warp check uses the same one). Temporary, deliberately
	// noisy [DS_COR_DROP] log for the operator's acceptance testing --
	// silence or remove once confirmed.
	// MT2009_PLUS_BOT_RARE_DROP_V1 (server-patches/botraredrop): a bot's kill rolls the
	// Cor Draconis like a player's; the bot lists it on its counter.
	// MT2009_PLUS_RARE_LEVEL_V1 (server-patches/rarelevel): no Cor Draconis from a
	// Metin or boss more than 15 levels below the killer (a level 90 at a level
	// 5 stone); a stronger one drops with no limit.
	if (pkKiller &&
		pkKiller->IsPC() &&
		pkKiller->GetDesc() &&
		pkKiller->GetLevel() <= pkChr->GetLevel() + 15 &&
		// MT2009_PLUS_RARE_TOGGLE_V1 (server-patches/raretoggle): no Cor Draconis
		// while the world has the alchemy switched off (M2_ALCHEMY=0, the panel).
		quest::CQuestManager::instance().GetEventFlag("m2_alchemy_off") == 0)
	{
		int corChance = 0;
		const char* corKind = "none";

		if (pkChr->IsStone())
		{
			// MT2009_PLUS_METIN_DROPS_V1 (server-patches/metindrops): a Metin 20% (it was 50).
			corChance = 20;
			corKind = "stone";
		}
		else if (pkChr->GetMobRank() >= MOB_RANK_BOSS)
		{
			corChance = 80;
			corKind = "boss";
		}

		// MT2009_PLUS_RARE_MOB_RULES_V1 (cor): the mob's own Cor chance and count.
		const Mt2009PlusRareMobRule* rareRule = Mt2009PlusRareMobRuleFor(pkChr);
		if (rareRule)
		{
			corChance = rareRule->corChance;
			corKind = "rule";
		}

		// MT2009_PLUS_BOT_RARE_DROP_V2: a bot's kill rolls at a bot's own chance.
		if (corChance > 0 && pkKiller->GetDesc()->IsBot())
			corChance = 5;

		if (corChance > 0)
		{
			int roll = number(1, 100);
			bool bWin = roll <= corChance;

			sys_log(0, "[DS_COR_DROP] mob_vnum=%u kind=%s killer=%s killer_pid=%u is_bot=%d chance=%d roll=%d win=%d",
				pkChr->GetRaceNum(), corKind, pkKiller->GetName(), pkKiller->GetPlayerID(),
				pkKiller->GetDesc() && pkKiller->GetDesc()->IsBot() ? 1 : 0, corChance, roll, bWin ? 1 : 0);

			if (bWin)
			{
				// MT2009_PLUS_RARE_MOB_RULES_V1 (cor count)
				LPITEM cor = CreateItem(50255, rareRule && rareRule->corCount > 1 ? rareRule->corCount : 1);

				sys_log(0, "[DS_COR_DROP] mob_vnum=%u kind=%s killer=%s killer_pid=%u created=%d",
					pkChr->GetRaceNum(), corKind, pkKiller->GetName(), pkKiller->GetPlayerID(), cor ? 1 : 0);

				// A bot's own Cor Draconis goes straight into its bag: a bot may
				// not pick one up off the ground (char_item.cpp, PickupItem).
				if (cor)
				{
					// MT2009_PLUS_BOT_RARE_DROP_V3: a full bag gets nothing, never the ground.
					if (pkKiller->GetDesc()->IsBot())
					{
						if (pkKiller->GetEmptyInventoryEx(cor) != -1)
							pkKiller->AutoGiveItem(cor);
						else
							M2_DESTROY_ITEM(cor);
					}
					else
						vec_item.push_back(cor);
				}
			}
		}
	}

	// Szarfa (COSTUME_ACCE) drops -- operator's explicit request, 17
	// September 2026. Same shape as the Cor Draconis block above: flat
	// chance, players only (a playerbot is still IsPC(), GetDesc()->IsBot()
	// is the real test), independent of GetDropPct()/level-delta/premium/
	// event/party. Two separate drops:
	//   - every boss (MOB_RANK_BOSS+), a "simple" (tier 1, "+0") sash, one
	//     of the 5 series picked at random, 80% flat.
	//   - boss treasure chests (opened via ITEM_TREASURE_BOX, char_item.cpp)
	//     are NOT mob kills and do not reach this function at all -- see
	//     the separate block in CHARACTER::UseItem/OpenTreasureBox for the
	//     "unique" (tier 4) chest drop instead.
	// MT2009_PLUS_BOT_RARE_DROP_V1: a bot's boss kill rolls the sash too.
	if (pkKiller &&
		pkKiller->IsPC() &&
		pkKiller->GetDesc() &&
		pkChr->GetMobRank() >= MOB_RANK_BOSS &&
		// MT2009_PLUS_RARE_LEVEL_V1: the sash, too, only within 15 levels below.
		pkKiller->GetLevel() <= pkChr->GetLevel() + 15 &&
		// MT2009_PLUS_RARE_TOGGLE_V1: no sash while the world has them switched off.
		quest::CQuestManager::instance().GetEventFlag("m2_sash_off") == 0)
	{
		// 85101 (Death Ruler Wings +0) added to the pool 17 September 2026,
		// operator's explicit request, alongside the classic Szarfa series --
		// same tier-1 shape (grade 1, refine_set 409, no bonus).
		static const DWORD s_aSzarfaTier1[] = { 85001, 85005, 85011, 85015, 85021};
		// MT2009_PLUS_BOT_SASH_DROP_V1 (server-patches/botsashdrop): a bot rolls at a
		// player's chance (it was 3, MT2009_PLUS_BOT_RARE_DROP_V2).
		// MT2009_PLUS_METIN_DROPS_V1 (server-patches/metindrops): a Metin 15%, a boss 80%.
		int szarfaChance = pkChr->IsStone() ? 15 : 80;
		// MT2009_PLUS_RARE_MOB_RULES_V1 (sash): the mob's own sash chance.
		if (const Mt2009PlusRareMobRule* sashRule = Mt2009PlusRareMobRuleFor(pkChr))
			szarfaChance = sashRule->sashChance;
		int roll = number(1, 100);
		bool bWin = roll <= szarfaChance;

		DWORD szarfaVnum = s_aSzarfaTier1[number(0, _countof(s_aSzarfaTier1) - 1)];

		sys_log(0, "[SZARFA_DROP] source=boss_kill mob_vnum=%u killer=%s killer_pid=%u chance=%d roll=%d win=%d vnum=%u",
			pkChr->GetRaceNum(), pkKiller->GetName(), pkKiller->GetPlayerID(), szarfaChance, roll, bWin ? 1 : 0, szarfaVnum);

		if (bWin)
		{
			LPITEM szarfa = CreateItem(szarfaVnum, 1);

			sys_log(0, "[SZARFA_DROP] source=boss_kill vnum=%u created=%d", szarfaVnum, szarfa ? 1 : 0);

			if (szarfa)
			{
				// MT2009_PLUS_BOT_RARE_DROP_V3: a full bag gets nothing, never the ground.
				if (pkKiller->GetDesc()->IsBot())
				{
					if (pkKiller->GetEmptyInventoryEx(szarfa) != -1)
						pkKiller->AutoGiveItem(szarfa);
					else
						M2_DESTROY_ITEM(szarfa);
				}
				else
					vec_item.push_back(szarfa);
			}
		}
	}

	// A Blessing Scroll (25040) from a Metin stone of level 15 to 99, for the
	// anvil and the counters ("zwoje wypadaja z Metinow na poziomach 15-99
	// testowo 1%", Iwakura): under level seventy nothing else drops one.
	// The permille is CONFIG, and a killer more than the book's fifteen
	// levels over the stone gets none.
	if (pkChr->IsStone() && pkKiller && g_iBlessingScrollStonePermille > 0 &&
			pkChr->GetLevel() >= PLAYERBOT_BLESSING_SCROLL_STONE_MIN_LEVEL &&
			pkChr->GetLevel() <= PLAYERBOT_BLESSING_SCROLL_STONE_MAX_LEVEL &&
			pkKiller->GetLevel() <= pkChr->GetLevel() + PLAYERBOT_METIN_BOOK_LEVEL_DELTA &&
			number(1, 1000) <= g_iBlessingScrollStonePermille)
	{
		item = CreateItem(25040, 1, 0, true);
		if (item) vec_item.emplace_back(item);
	}

	if (pkKiller->IsHorseRiding() &&
			GetDropPerKillPct(1000, 1000000, iDeltaPercent, "horse_skill_book_drop") >= number(1, iRandRange))
	{
		sys_log(0, "EVENT HORSE_SKILL_BOOK_DROP");

		if ((item = CreateItem(ITEM_HORSE_SKILL_TRAIN_BOOK, 1, 0, true)))
			vec_item.emplace_back(item);
	}

	// MT2009_PLUS_LOOT_EVENTS_V1: the double-loot events (playerbot_events.h):
	// while one runs, every item a boss or a Metin stone dropped so far is
	// dropped once more - a fresh item of the same vnum and count. Quest drops
	// below are left single.
	{
		bool Mt2009PlusDoubleLoot(LPCHARACTER victim);
		if (Mt2009PlusDoubleLoot(pkChr))
		{
			const size_t dropped = vec_item.size();
			for (size_t i = 0; i < dropped; ++i)
			{
				LPITEM twin = CreateItem(vec_item[i]->GetVnum(), vec_item[i]->GetCount(), 0, true);
				if (twin)
					vec_item.emplace_back(twin);
			}
		}
	}
	} // MT2009_PLUS_DUNGEON_DROP_V1 (specials end)
	// MT2009_PLUS_SEONHAE_V1 (drop): Seon-Hae's shards and additives from Metins and
	// bosses on the progression maps, for a real player only (playerbot_seonhae.h,
	// /opt/m2spool/seonhae_drops.tsv); after the double-loot events, so never doubled.
	{
		void Mt2009PlusSeonHaeDrop(LPCHARACTER victim, LPCHARACTER killer, std::vector<LPITEM>& out);
		Mt2009PlusSeonHaeDrop(pkChr, pkKiller, vec_item);
	}
	CreateQuestDropItem(pkChr, pkKiller, vec_item, iDeltaPercent, iRandRange);

	for (itertype(vec_item) it = vec_item.begin(); it != vec_item.end(); ++it)
	{
		LPITEM item = *it;
		DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, item->GetVnum(), item->GetCount());
	}

	return vec_item.size();
}

// ADD_GRANDMASTER_SKILL
int GetThreeSkillLevelAdjust(int level)
{
	if (level < 40)
		return 32;
	if (level < 45)
		return 16;
	if (level < 50)
		return 8;
	if (level < 55)
		return 4;
	if (level < 60)
		return 2;
	return 1;
}
// END_OF_ADD_GRANDMASTER_SKILL

// DROPEVENT_CHARSTONE
// drop_char_stone 1
// drop_char_stone.percent_lv01_10 5
// drop_char_stone.percent_lv11_30 10
// drop_char_stone.percent_lv31_MX 15
// drop_char_stone.level_range	   10
static struct DropEvent_CharStone
{
	int percent_lv01_10;
	int percent_lv11_30;
	int percent_lv31_MX;
	int level_range;
	bool alive;

	DropEvent_CharStone()
	{
		percent_lv01_10 =  100;
		percent_lv11_30 =  200;
		percent_lv31_MX =  300;
		level_range = 10;
		alive = false;
	}
} gs_dropEvent_charStone;

static int __DropEvent_CharStone_GetDropPercent(int killer_level)
{
	int killer_levelStep = (killer_level-1)/10;

	switch (killer_levelStep)
	{
		case 0:
			return gs_dropEvent_charStone.percent_lv01_10;

		case 1:
		case 2:
			return gs_dropEvent_charStone.percent_lv11_30;
	}

	return gs_dropEvent_charStone.percent_lv31_MX;
}

static void __DropEvent_CharStone_DropItem(CHARACTER & killer, CHARACTER & victim, ITEM_MANAGER& itemMgr, std::vector<LPITEM>& vec_item)
{
	if (!gs_dropEvent_charStone.alive)
		return;

	int killer_level = killer.GetLevel();
	int dropPercent = __DropEvent_CharStone_GetDropPercent(killer_level);

	int MaxRange = 10000;

	if (number(1, MaxRange) <= dropPercent)
	{
		int log_level = (test_server || killer.GetGMLevel() >= GM_LOW_WIZARD) ? 0 : 1;
		int victim_level = victim.GetLevel();
		int level_diff = victim_level - killer_level;

		if (level_diff >= +gs_dropEvent_charStone.level_range || level_diff <= -gs_dropEvent_charStone.level_range)
		{
			sys_log(log_level,
					"dropevent.drop_char_stone.level_range_over: killer(%s: lv%d), victim(%s: lv:%d), level_diff(%d)",
					killer.GetName(), killer.GetLevel(), victim.GetName(), victim.GetLevel(), level_diff);
			return;
		}

		static const int Stones[] = { 30210, 30211, 30212, 30213, 30214, 30215, 30216, 30217, 30218, 30219, 30258, 30259, 30260, 30261, 30262, 30263 };
		int item_vnum = Stones[number(0, _countof(Stones)-1)]; // @fixme189

		LPITEM p_item = NULL;

		if ((p_item = itemMgr.CreateItem(item_vnum, 1, 0, true)))
		{
			vec_item.emplace_back(p_item);

			sys_log(log_level,
					"dropevent.drop_char_stone.item_drop: killer(%s: lv%d), victim(%s: lv:%d), item_name(%s)",
					killer.GetName(), killer.GetLevel(), victim.GetName(), victim.GetLevel(), p_item->GetName());
		}
	}
}

bool DropEvent_CharStone_SetValue(const std::string& name, int value)
{
	if (name == "drop_char_stone")
	{
		gs_dropEvent_charStone.alive = value;

		if (value)
			sys_log(0, "dropevent.drop_char_stone = on");
		else
			sys_log(0, "dropevent.drop_char_stone = off");

	}
	else if (name == "drop_char_stone.percent_lv01_10")
		gs_dropEvent_charStone.percent_lv01_10 = value;
	else if (name == "drop_char_stone.percent_lv11_30")
		gs_dropEvent_charStone.percent_lv11_30 = value;
	else if (name == "drop_char_stone.percent_lv31_MX")
		gs_dropEvent_charStone.percent_lv31_MX = value;
	else if (name == "drop_char_stone.level_range")
		gs_dropEvent_charStone.level_range = value;
	else
		return false;

	sys_log(0, "dropevent.drop_char_stone: %d", gs_dropEvent_charStone.alive ? true : false);
	sys_log(0, "dropevent.drop_char_stone.percent_lv01_10: %f", gs_dropEvent_charStone.percent_lv01_10/100.0f);
	sys_log(0, "dropevent.drop_char_stone.percent_lv11_30: %f", gs_dropEvent_charStone.percent_lv11_30/100.0f);
	sys_log(0, "dropevent.drop_char_stone.percent_lv31_MX: %f", gs_dropEvent_charStone.percent_lv31_MX/100.0f);
	sys_log(0, "dropevent.drop_char_stone.level_range: %d", gs_dropEvent_charStone.level_range);

	return true;
}

// END_OF_DROPEVENT_CHARSTONE

// fixme

static struct DropEvent_RefineBox
{
	int percent_low;
	int low;
	int percent_mid;
	int mid;
	int percent_high;
	//int level_range;
	bool alive;

	DropEvent_RefineBox()
	{
		percent_low =  100;
		low = 20;
		percent_mid =  100;
		mid = 45;
		percent_high =  100;
		//level_range = 10;
		alive = false;
	}
} gs_dropEvent_refineBox;

static LPITEM __DropEvent_RefineBox_GetDropItem(CHARACTER & killer, CHARACTER & victim, ITEM_MANAGER& itemMgr)
{
	static const int lowerBox[] = { 50197, 50198, 50199 };
	static const int lowerBox_range = 3;
	static const int midderBox[] = { 50203, 50204, 50205, 50206 };
	static const int midderBox_range = 4;
	static const int higherBox[] = { 50207, 50208, 50209, 50210, 50211 };
	static const int higherBox_range = 5;

	if (victim.GetMobRank() < MOB_RANK_KNIGHT)
		return NULL;

	int killer_level = killer.GetLevel();
	//int level_diff = victim_level - killer_level;

	//if (level_diff >= +gs_dropEvent_refineBox.level_range || level_diff <= -gs_dropEvent_refineBox.level_range)
	//{
	//	sys_log(log_level,
	//		"dropevent.drop_refine_box.level_range_over: killer(%s: lv%d), victim(%s: lv:%d), level_diff(%d)",
	//		killer.GetName(), killer.GetLevel(), victim.GetName(), victim.GetLevel(), level_diff);
	//	return NULL;
	//}

	if (killer_level <= gs_dropEvent_refineBox.low)
	{
		if (number (1, gs_dropEvent_refineBox.percent_low) == 1)
		{
			return itemMgr.CreateItem(lowerBox [number (1,lowerBox_range) - 1], 1, 0, true);
		}
	}
	else if (killer_level <= gs_dropEvent_refineBox.mid)
	{
		if (number (1, gs_dropEvent_refineBox.percent_mid) == 1)
		{
			return itemMgr.CreateItem(midderBox [number (1,midderBox_range) - 1], 1, 0, true);
		}
	}
	else
	{
		if (number (1, gs_dropEvent_refineBox.percent_high) == 1)
		{
			return itemMgr.CreateItem(higherBox [number (1,higherBox_range) - 1], 1, 0, true);
		}
	}
	return NULL;
}

static void __DropEvent_RefineBox_DropItem(CHARACTER & killer, CHARACTER & victim, ITEM_MANAGER& itemMgr, std::vector<LPITEM>& vec_item)
{
	if (!gs_dropEvent_refineBox.alive)
		return;

	int log_level = (test_server || killer.GetGMLevel() >= GM_LOW_WIZARD) ? 0 : 1;

	LPITEM p_item = __DropEvent_RefineBox_GetDropItem(killer, victim, itemMgr);

	if (p_item)
	{
		vec_item.emplace_back(p_item);

		sys_log(log_level,
			"dropevent.drop_refine_box.item_drop: killer(%s: lv%d), victim(%s: lv:%d), item_name(%s)",
			killer.GetName(), killer.GetLevel(), victim.GetName(), victim.GetLevel(), p_item->GetName());
	}
}

bool DropEvent_RefineBox_SetValue(const std::string& name, int value)
{
	if (name == "refine_box_drop")
	{
		gs_dropEvent_refineBox.alive = value;

		if (value)
			sys_log(0, "refine_box_drop = on");
		else
			sys_log(0, "refine_box_drop = off");

	}
	else if (name == "refine_box_low")
		gs_dropEvent_refineBox.percent_low = value < 100 ? 100 : value;
	else if (name == "refine_box_mid")
		gs_dropEvent_refineBox.percent_mid = value < 100 ? 100 : value;
	else if (name == "refine_box_high")
		gs_dropEvent_refineBox.percent_high = value < 100 ? 100 : value;
	//else if (name == "refine_box_level_range")
	//	gs_dropEvent_refineBox.level_range = value;
	else
		return false;

	sys_log(0, "refine_box_drop: %d", gs_dropEvent_refineBox.alive ? true : false);
	sys_log(0, "refine_box_low: %d", gs_dropEvent_refineBox.percent_low);
	sys_log(0, "refine_box_mid: %d", gs_dropEvent_refineBox.percent_mid);
	sys_log(0, "refine_box_high: %d", gs_dropEvent_refineBox.percent_high);
	//sys_log(0, "refine_box_low_level_range: %d", gs_dropEvent_refineBox.level_range);

	return true;
}

void ITEM_MANAGER::CreateQuestDropItem(LPCHARACTER pkChr, LPCHARACTER pkKiller, std::vector<LPITEM> & vec_item, int iDeltaPercent, int iRandRange)
{
	LPITEM item = NULL;

	if (!pkChr)
		return;

	if (!pkKiller)
		return;

	sys_log(1, "CreateQuestDropItem victim(%s), killer(%s)", pkChr->GetName(), pkKiller->GetName() );

	int deltaLevel = pkKiller->GetLevel() - pkChr->GetLevel();
	int absDeltaLevel = abs(deltaLevel);

	// DROPEVENT_CHARSTONE
	__DropEvent_CharStone_DropItem(*pkKiller, *pkChr, *this, vec_item);
	// END_OF_DROPEVENT_CHARSTONE
	__DropEvent_RefineBox_DropItem(*pkKiller, *pkChr, *this, vec_item);
	// MT2009_PLUS_RUMI_V1 (kill): an Okey card per kill share while the event runs
	// (playerbot_rumi.h, Owsap's __OKEY_EVENT_FLAG_RENEWAL__: a counter, no item on the ground).
	{
		void RumiOnKill(LPCHARACTER killer, LPCHARACTER victim, int iDeltaPercent, int iRandRange);
		RumiOnKill(pkKiller, pkChr, iDeltaPercent, iRandRange);
	}

	if (quest::CQuestManager::instance().GetEventFlag("xmas_sock"))
	{
		const DWORD SOCK_ITEM_VNUM = 50010;

		int iDropPerKill[MOB_RANK_MAX_NUM] =
		{
			2000,
			1000,
			300,
			50,
			0,
			0,
		};

		if ( iDropPerKill[pkChr->GetMobRank()] != 0 )
		{
			int iPercent = 40000 * iDeltaPercent / iDropPerKill[pkChr->GetMobRank()];

			sys_log(0, "SOCK DROP %d %d", iPercent, iRandRange);
			if (iPercent >= number(1, iRandRange))
			{
				if ((item = CreateItem(SOCK_ITEM_VNUM, 1, 0, true)))
					vec_item.emplace_back(item);
			}
		}
	}

	if (quest::CQuestManager::instance().GetEventFlag("drop_moon"))
	{
		const DWORD ITEM_VNUM = 50011;

		int iDropPerKill[MOB_RANK_MAX_NUM] =
		{
			2000,
			1000,
			300,
			50,
			0,
			0,
		};

		if (iDropPerKill[pkChr->GetMobRank()])
		{
			int iPercent = 40000 * iDeltaPercent / iDropPerKill[pkChr->GetMobRank()];

			if (iPercent >= number(1, iRandRange))
			{
				if ((item = CreateItem(ITEM_VNUM, 1, 0, true)))
					vec_item.emplace_back(item);
			}
		}
	}

	if (pkKiller->GetLevel() >= 15 && absDeltaLevel <= 5)
	{
		int pct = quest::CQuestManager::instance().GetEventFlag("hc_drop");

		if (pct > 0)
		{
			const DWORD ITEM_VNUM = 30178;

			if (number(1,100) <= pct)
			{
				if ((item = CreateItem(ITEM_VNUM, 1, 0, true)))
					vec_item.emplace_back(item);
			}
		}
	}

	if (quest::CQuestManager::instance().GetEventFlag("valentine_event"))
	{
		if (pkKiller->GetLevel() >= 20 && deltaLevel < 10)
		{
			int pct = MIN(quest::CQuestManager::instance().GetEventFlag("valentine_drop"), 500); // MAX 5%
			if (pct > 0)
			{
				if (pkKiller->GetDungeon())
					pct /= 2;

				if (number(1, 10000) <= pct)
				{
					const static DWORD valentine_items[2] = { 50024, 50025 };
					DWORD dwVnum = valentine_items[number(0, 1)];
					if ((item = CreateItem(dwVnum, 1, 0, true)))
						vec_item.emplace_back(item);
				}
			}
		}
	}

	if (quest::CQuestManager::instance().GetEventFlag("hex_drop"))
	{
		if (pkKiller->GetLevel() >= 20 && deltaLevel < 10)
		{
			int pct = MIN(quest::CQuestManager::instance().GetEventFlag("hex_drop"), 500); // MAX 5%
			if (pct > 0)
			{
				if (pkKiller->GetDungeon())
					pct /= 2;

				if (number(1, 10000) <= pct)
				{
					DWORD dwVnum = 50037;
					if ((item = CreateItem(dwVnum, 1, 0, true)))
						vec_item.emplace_back(item);
				}
			}
		}
	}

	// MT2009_PLUS_YUTNORI_V1 (drop): a birch branch for Yut Nori - a counter, not an item
	// on the ground (Owsap's __YUTNORI_EVENT_FLAG_RENEWAL__), real players only
	// (playerbot_yutnori.h).
	{
		void YutnoriKillRoll(LPCHARACTER killer, int iDeltaPercent, int iRandRange);
		YutnoriKillRoll(pkKiller, iDeltaPercent, iRandRange);
	}

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "2007_drop") >= number(1, iRandRange))
	{
		sys_log(0, "육각보합 DROP EVENT ");

		const static DWORD dwVnum = 50043;

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.emplace_back(item);
	}

	if (GetDropPerKillPct(/* minimum */ 100, /* default */ 1000, iDeltaPercent, "newyear_fire") >= number(1, iRandRange))
	{
		const DWORD ITEM_VNUM_FIRE = 50107;

		if ((item = CreateItem(ITEM_VNUM_FIRE, 1, 0, true)))
			vec_item.emplace_back(item);
	}

	if (GetDropPerKillPct(100, 500, iDeltaPercent, "newyear_moon") >= number(1, iRandRange))
	{
		sys_log(0, "EVENT NEWYEAR_MOON DROP");

		const static DWORD wonso_items[6] = { 50016, 50017, 50018, 50019, 50019, 50019, };
		DWORD dwVnum = wonso_items[number(0,5)];

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.emplace_back(item);
	}

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "icecream_drop") >= number(1, iRandRange))
	{
		const static DWORD icecream = 50123;

		if ((item = CreateItem(icecream, 1, 0, true)))
			vec_item.emplace_back(item);
	}

	if ((pkKiller->CountSpecifyItem(53002) > 0) && (GetDropPerKillPct(50, 100, iDeltaPercent, "new_xmas_event") >= number(1, iRandRange)))
	{
		const static DWORD xmas_sock = 50010;
		pkKiller->AutoGiveItem (xmas_sock, 1);
	}

	if ((pkKiller->CountSpecifyItem(53007) > 0) && (GetDropPerKillPct(50, 100, iDeltaPercent, "new_xmas_event") >= number(1, iRandRange)))
	{
		const static DWORD xmas_sock = 50010;
		pkKiller->AutoGiveItem (xmas_sock, 1);
	}

	//if (pkChr->GetLevel() >= 30 && (GetDropPerKillPct(50, 100, iDeltaPercent, "ds_drop") >= number(1, iRandRange)))
	//{
	//	const static DWORD dragon_soul_gemstone = 30270;
	//	if ((item = CreateItem(dragon_soul_gemstone, 1, 0, true)))
	//		vec_item.emplace_back(item);
	//}

	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "halloween_drop") >= number(1, iRandRange) )
	{
		const static DWORD halloween_item = 30321;

		if ( (item=CreateItem(halloween_item, 1, 0, true)) )
			vec_item.emplace_back(item);
	}

	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "ramadan_drop") >= number(1, iRandRange) )
	{
		const static DWORD ramadan_item = 30315;

		if ( (item=CreateItem(ramadan_item, 1, 0, true)) )
			vec_item.emplace_back(item);
	}

	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "easter_drop") >= number(1, iRandRange) )
	{
		const static DWORD easter_item_base = 50160;

		if ( (item=CreateItem(easter_item_base+number(0,19), 1, 0, true)) )
			vec_item.emplace_back(item);
	}

	if ( GetDropPerKillPct(100, 2000, iDeltaPercent, "football_drop") >= number(1, iRandRange) )
	{
		const static DWORD football_item = 50096;

		if ( (item=CreateItem(football_item, 1, 0, true)) )
			vec_item.emplace_back(item);
	}

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "whiteday_drop") >= number(1, iRandRange))
	{
		sys_log(0, "EVENT WHITEDAY_DROP");
		const static DWORD whiteday_items[2] = { ITEM_WHITEDAY_ROSE, ITEM_WHITEDAY_CANDY };
		DWORD dwVnum = whiteday_items[number(0,1)];

		if ((item = CreateItem(dwVnum, 1, 0, true)))
			vec_item.emplace_back(item);
	}

	if (pkKiller->GetLevel()>=50)
	{
		if (GetDropPerKillPct(100, 1000, iDeltaPercent, "kids_day_drop_high") >= number(1, iRandRange))
		{
			DWORD ITEM_QUIZ_BOX = 50034;

			if ((item = CreateItem(ITEM_QUIZ_BOX, 1, 0, true)))
				vec_item.emplace_back(item);
		}
	}
	else
	{
		if (GetDropPerKillPct(100, 1000, iDeltaPercent, "kids_day_drop") >= number(1, iRandRange))
		{
			DWORD ITEM_QUIZ_BOX = 50034;

			if ((item = CreateItem(ITEM_QUIZ_BOX, 1, 0, true)))
				vec_item.emplace_back(item);
		}
	}

	if (pkChr->GetLevel() >= 30 && GetDropPerKillPct(50, 100, iDeltaPercent, "medal_part_drop") >= number(1, iRandRange))
	{
		const static DWORD drop_items[] = { 30265, 30266, 30267, 30268, 30269 };
		int i = number (0, 4);
		item = CreateItem(drop_items[i]);
		if (item != NULL)
			vec_item.emplace_back(item);
	}

	// ADD_GRANDMASTER_SKILL

	if (pkChr->GetLevel() >= 40 && pkChr->GetMobRank() >= MOB_RANK_BOSS && GetDropPerKillPct(/* minimum */ 1, /* default */ 1000, iDeltaPercent, "three_skill_item") / GetThreeSkillLevelAdjust(pkChr->GetLevel()) >= number(1, iRandRange))
	{
		const DWORD ITEM_VNUM = 50513;

		if ((item = CreateItem(ITEM_VNUM, 1, 0, true)))
			vec_item.emplace_back(item);
	}
	// END_OF_ADD_GRANDMASTER_SKILL

	if (GetDropPerKillPct(100, 1000, iDeltaPercent, "dragon_boat_festival_drop") >= number(1, iRandRange))
	{
		const DWORD ITEM_SEED = 50085;

		if ((item = CreateItem(ITEM_SEED, 1, 0, true)))
			vec_item.emplace_back(item);
	}

	// MT2009_PLUS_CATCH_KING_V1 (drop): a King Card for a share of the kill while Catch the King
	// runs (mini_game_catchking_drop; players only, never a bot - playerbot_catchking.h).
	{
		void CatchKingOnKill(LPCHARACTER victim, LPCHARACTER killer, int iDeltaPercent, int iRandRange);
		CatchKingOnKill(pkChr, pkKiller, iDeltaPercent, iRandRange);
	}

	if (pkKiller->GetLevel() >= 15 && quest::CQuestManager::instance().GetEventFlag("mars_drop"))
	{
		const DWORD ITEM_HANIRON = 70035;
		int iDropMultiply[MOB_RANK_MAX_NUM] =
		{
			50,
			30,
			5,
			1,
			0,
			0,
		};

		if (iDropMultiply[pkChr->GetMobRank()] &&
				GetDropPerKillPct(1000, 1500, iDeltaPercent, "mars_drop") >= number(1, iRandRange) * iDropMultiply[pkChr->GetMobRank()])
		{
			if ((item = CreateItem(ITEM_HANIRON, 1, 0, true)))
				vec_item.emplace_back(item);
		}
	}

	// MT2009_PLUS_FLOWER_V1 (kill): the Flower Event's seeds - a counter, not an
	// item on the ground; players only, while the event runs (playerbot_flower.h).
	{
		void FlowerEventOnKill(LPCHARACTER victim, LPCHARACTER killer, int iDeltaPercent, int iRandRange);
		FlowerEventOnKill(pkChr, pkKiller, iDeltaPercent, iRandRange);
	}
}

void ITEM_MANAGER::ListSpecialItemDrop(LPCHARACTER ch, DWORD vnum)
{
	auto t = GetTable(vnum);
	if (!t)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "no drop info for %d", vnum);
		return;
	}

	auto it = m_map_pkSpecialItemGroup.find(vnum);
	if (it != m_map_pkSpecialItemGroup.end())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "### SPECIAL ITEM DROP FOR %d ###", vnum);
		auto vec = it->second->GetItems();
		auto probs = it->second->GetProbs();
		int i = 0;
		for (const auto& item : vec)
		{
			auto itemTable = GetTable(item.vnum);
			if (item.vnum < 10 || itemTable)
			{
				int pool_sum = probs.back();
				int prob_before = i > 0 ? probs[i - 1] : 0;
				int prob_this = probs[i];
				float prob_for_item = (float)(prob_this - prob_before) / pool_sum;

				std::string itemName = "UNKNOWN";
				if (item.vnum == CSpecialItemGroup::POISON)
					itemName = "poison";
				else if (item.vnum == CSpecialItemGroup::EXP)
					itemName = "exp";
				else if (item.vnum == CSpecialItemGroup::GOLD)
					itemName = "gold";
				else if (item.vnum == CSpecialItemGroup::MOB)
					itemName = "mob";
				else if (item.vnum == CSpecialItemGroup::SLOW)
					itemName = "slow";
				else if (item.vnum == CSpecialItemGroup::DRAIN_HP)
					itemName = "drain_hp";
				else if (item.vnum == CSpecialItemGroup::MOB_GROUP)
					itemName = "group";
				else if (itemTable)
					itemName = itemTable->szLocaleName;

				ch->ChatPacket(CHAT_TYPE_INFO, "%dx %s (%d) %.2f%%", item.count, itemName.c_str(), item.vnum, prob_for_item * 100);
			}
			i++;
		}
		ch->ChatPacket(CHAT_TYPE_INFO, "######");
	}
}

void ITEM_MANAGER::ListMobItemDrop(LPCHARACTER ch, DWORD mobVnum)
{
	const CMob* pkMob = CMobManager::instance().Get(mobVnum);
	if (!pkMob)
		return;

	int iDeltaPercent, iRandRange;
	if (!GetDropPct(ch, mobVnum, pkMob->m_table.bRank, pkMob->m_table.bLevel, pkMob->m_table.bType == CHAR_TYPE_STONE, iDeltaPercent, iRandRange))
		return;

	ch->ChatPacket(CHAT_TYPE_INFO, "### DROP VALUES FOR CHARACTER deltaPercent %d randRange %d ###", iDeltaPercent, iRandRange);

	ch->ChatPacket(CHAT_TYPE_INFO, "### DROP FOR MOB %d ###", mobVnum);
	// TYPE DROP
	auto dropIt = m_map_pkDropItemGroup.find(mobVnum);
	if (dropIt != m_map_pkDropItemGroup.end())
	{
		auto vec = dropIt->second->GetVector();
		ch->ChatPacket(CHAT_TYPE_INFO, "### DROP TYPE ###");
		for (const auto& item : vec)
		{
			auto itemTable = GetTable(item.dwVnum);
			if (itemTable)
			{
				float prob = (float)(item.dwPct * iDeltaPercent / 100) / iRandRange;
				ch->ChatPacket(CHAT_TYPE_INFO, "%dx %s (%d) %.2f%%", item.iCount, itemTable->szLocaleName, item.dwVnum, prob * 100);
			}
		}
		ch->ChatPacket(CHAT_TYPE_INFO, "######");
	}


	// TYPE KILL
	auto killIt = m_map_pkMobItemGroup.find(mobVnum);
	if (killIt != m_map_pkMobItemGroup.end())
	{
		auto vec = killIt->second->GetItems();
		ch->ChatPacket(CHAT_TYPE_INFO, "### KILL TYPE ###");
		ch->ChatPacket(CHAT_TYPE_INFO, "KILL PER DROP: %d", killIt->second->GetKillPerDrop());
		float first_chance = (float)(40000 * iDeltaPercent / killIt->second->GetKillPerDrop()) / iRandRange;
		//ch->ChatDebug("first_chance %f", first_chance);
		auto probs = killIt->second->GetProbs();
		int i = 0;
		for (const auto& item : vec)
		{
			auto itemTable = GetTable(item.dwItemVnum);
			if (itemTable)
			{
				int pool_sum = probs.back();
				int prob_before = i > 0 ? probs[i - 1] : 0;
				int prob_this = probs[i];
				float prob_for_item = (float)(prob_this - prob_before) / pool_sum;
				float general_prob = prob_for_item * first_chance;
				//ch->ChatDebug("%d %d %d %f %f", prob_this, prob_before, pool_sum, prob_for_item, general_prob);
				ch->ChatPacket(CHAT_TYPE_INFO, "%dx %s (%d) %.2f%%", item.iCount, itemTable->szLocaleName, item.dwItemVnum, general_prob * 100);
			}
			i++;
		}
		ch->ChatPacket(CHAT_TYPE_INFO, "######");
	}

	// TYPE LIMIT
	auto levelIt = m_map_pkLevelItemGroup.find(mobVnum);
	if (levelIt != m_map_pkLevelItemGroup.end())
	{
		auto vec = levelIt->second->GetVector();
		ch->ChatPacket(CHAT_TYPE_INFO, "### LIMIT TYPE ###");
		ch->ChatPacket(CHAT_TYPE_INFO, "LEVEL LIMIT: %d", levelIt->second->GetLevelLimit());
		for (const auto& item : vec)
		{
			auto itemTable = GetTable(item.dwVNum);
			if (itemTable)
			{
				float prob = 0;
				if (ch->GetLevel() >= levelIt->second->GetLevelLimit())
					prob = (float)item.dwPct / 1000000;
				ch->ChatPacket(CHAT_TYPE_INFO, "%dx %s (%d) %.2f%%", item.iCount, itemTable->szLocaleName, item.dwVNum, prob * 100);
			}
		}
		ch->ChatPacket(CHAT_TYPE_INFO, "######");
	}


	auto etcIt = m_map_dwEtcItemDropProb.find(pkMob->m_table.dwDropItemVnum);
	if (etcIt != m_map_dwEtcItemDropProb.end())
	{
		ch->ChatPacket(CHAT_TYPE_INFO, "### ETC TYPE ###");
		auto itemTable = GetTable(etcIt->first);
		if (itemTable)
		{
			float prob = (float)(etcIt->second * iDeltaPercent / 100) / iRandRange;
			ch->ChatPacket(CHAT_TYPE_INFO, "1x %s (%d) %.2f%%", itemTable->szLocaleName, etcIt->first, prob * 100);
		}
		ch->ChatPacket(CHAT_TYPE_INFO, "######");
	}
}

// Mob drop preview (custom-patches/mob_drop_preview.patch), protocol v1.
//
// Every mob vnum whose "when <vnum>.kill" handler exists anywhere in the
// active quest tree (linux-port/docker/game/src/serverfiles/share/locale/
// poland/quest/quest/, excluding _unused/) -- found 15 September 2026 with:
//   grep -rhoE 'when[[:space:]]+[0-9]+\.kill' quest/ | grep -oE '[0-9]+' | sort -nu
// Most of these handlers only advance a quest counter and grant nothing;
// a few (event_easter's metin_killed among them) call pc.give_item2 /
// game.drop_item_with_ownership with items this function cannot see. There
// is no registry of which is which without parsing each quest's Lua body,
// so every vnum below forces out_complete=false rather than risk a false
// "this is everything" -- regenerate this list (same grep) if the quest
// tree changes, and re-check any vnum added here for what it actually
// grants if a tighter answer is ever wanted.
static const DWORD s_anQuestKillHookVnums[] = {
	101, 104, 105, 106, 107, 108, 109, 110, 113, 114, 144, 152, 173, 174, 175,
	177, 181, 182, 183, 191, 193, 301, 302, 391, 392, 394, 402, 404, 405, 406,
	493, 501, 502, 503, 504, 505, 551, 591, 601, 603, 631, 633, 634, 635, 636,
	691, 701, 702, 703, 705, 706, 707, 731, 734, 737, 792, 793, 903, 906, 1001,
	1091, 1092, 1093, 1101, 1107, 1137, 1192, 1302, 1303, 1304, 1305, 1306,
	1401, 1901, 1902, 2001, 2034, 2036, 2091, 2092, 2102, 2106, 2108, 2191,
	2202, 2203, 2205, 2206, 2281, 2291, 2301, 2305, 2306, 2307, 2311, 2317,
	2401, 2403, 2493, 2501, 2502, 2503, 3091, 3191, 3291, 3391, 3491, 3591,
	3595, 3691, 3791, 3891, 5002, 5121, 5125, 5161, 5162, 5163, 8001, 8002,
	8003, 8004, 8005, 8006, 8007, 8008, 8009, 8010, 8011, 8012, 8013, 8014,
	8024, 8025, 8026, 8027, 8031, 8041, 8042, 8043, 8044, 8045, 8046, 8047,
	8048, 8049, 8050, 8051, 8052, 8053, 8054, 8055, 8056,
};

bool ITEM_MANAGER::HasQuestKillHook(DWORD mobVnum)
{
	static std::set<DWORD> s_set;
	if (s_set.empty())
	{
		for (size_t i = 0; i < _countof(s_anQuestKillHookVnums); ++i)
			s_set.insert(s_anQuestKillHookVnums[i]);
	}
	return s_set.count(mobVnum) > 0;
}

// Exhaustive, non-random walk of a CSpecialItemGroup: every entry AddItem()
// kept had a positive weight already (AddItem refuses prob==0), so unlike
// GetSpecialItemGroupDropInfo() (which calls GetOneIndex() -- a real roll)
// nothing here needs a probability check, only the isSpecial recursion
// GetSpecialItemGroupDropInfo() itself does, walked over every branch
// instead of the one index() would pick. depth guards against a group
// referencing itself; none should, but this function must never touch the
// RNG or loop forever finding out.
static void __CollectSpecialItemGroup(ITEM_MANAGER & mgr, DWORD dwGroupVnum, std::set<DWORD> & out_vnums, int depth = 0)
{
	if (depth > 8)
		return;

	const CSpecialItemGroup * pGroup = mgr.GetSpecialItemGroup(dwGroupVnum);
	if (!pGroup)
		return;

	std::vector<CSpecialItemGroup::CSpecialItemInfo> items = pGroup->GetItems();
	for (size_t i = 0; i < items.size(); ++i)
	{
		const CSpecialItemGroup::CSpecialItemInfo & info = items[i];
		if (info.isSpecial)
		{
			__CollectSpecialItemGroup(mgr, info.vnum, out_vnums, depth + 1);
			continue;
		}
		// Sentinel effect codes (GOLD/EXP/MOB/SLOW/DRAIN_HP/POISON/MOB_GROUP)
		// share the enum's small integer range and are never real items --
		// same guard ListSpecialItemDrop() already uses above.
		if (info.vnum < 10)
			continue;
		if (mgr.GetTable(info.vnum))
			out_vnums.insert(info.vnum);
	}
}

void ITEM_MANAGER::GetPossibleMobDropItems(LPCHARACTER pkKiller, LPCHARACTER pkChr, std::set<DWORD> & out_vnums, bool & out_complete)
{
	out_vnums.clear();
	out_complete = true;

	if (!pkKiller || !pkChr)
	{
		out_complete = false;
		return;
	}

	DWORD race = pkChr->GetRaceNum();
	if (HasQuestKillHook(race))
		out_complete = false;

	int iDeltaPercent, iRandRange;
	if (!GetDropPct(pkKiller, race, pkChr->GetMobRank(), pkChr->GetLevel(), pkChr->IsStone(), iDeltaPercent, iRandRange))
	{
		out_complete = false;
		return;
	}

	// MT2009_PLUS_DROP_PREVIEW_MIN_V1 (server-patches/droppreview): an item
	// rolled against iRandRange is listed only at 1 in 10 000 kills or better,
	// not at 1 in 4 000 000 - the common drop goes by the killer's level, and a
	// level-35 player saw +2 weapons on a level-1 dog. The drop is unchanged.
	const int iPreviewMinPercent = MAX(1, iRandRange / 10000);

	int iLevel = pkKiller->GetLevel();
	BYTE bRank = pkChr->GetMobRank();

	// Common Drop Items -- per-rank, filtered by the killer's own level
	// range. iRandRange only ever scales the roll target, never zero, so a
	// branch is possible exactly when its own percent (after the level-delta
	// multiplier) is at least 1.
	{
		const std::vector<CItemDropInfo> & vec = g_vec_pkCommonDropItem[bRank];
		for (size_t i = 0; i < vec.size(); ++i)
		{
			const CItemDropInfo & info = vec[i];
			if (iLevel < info.m_iLevelStart || iLevel > info.m_iLevelEnd)
				continue;

			int iPercent = (info.m_iPercent * iDeltaPercent) / 100;
			if (iPercent < iPreviewMinPercent)
				continue;

			TItemTable * table = GetTable(info.m_dwVnum);
			if (!table)
				continue;

			if (table->bType == ITEM_POLYMORPH)
			{
				// CreateDropItem only ever creates the polymorph matching
				// this exact victim's own race, never any other polymorph
				// entry that happens to share this rank's common table.
				if (info.m_dwVnum == pkChr->GetPolymorphItemVnum())
					out_vnums.insert(info.m_dwVnum);
				continue;
			}

			out_vnums.insert(info.m_dwVnum);
		}
	}

	// Drop Item Group (race-specific), with special-group entries walked
	// exhaustively instead of resolved to the one CreateDropItem() would roll.
	{
		auto it = m_map_pkDropItemGroup.find(race);
		if (it != m_map_pkDropItemGroup.end())
		{
			const auto & vec = it->second->GetVector();
			for (size_t i = 0; i < vec.size(); ++i)
			{
				const auto & info = vec[i];
				int iPercent = (info.dwPct * iDeltaPercent) / 100;
				if (iPercent < iPreviewMinPercent)
					continue;

				if (info.bSpecialGroup)
				{
					__CollectSpecialItemGroup(*this, info.dwVnum, out_vnums);
					continue;
				}

				TItemTable * table = GetTable(info.dwVnum);
				if (!table)
					continue;

				if (table->bType == ITEM_POLYMORPH && info.dwVnum != pkChr->GetPolymorphItemVnum())
					continue;

				out_vnums.insert(info.dwVnum);
			}
		}
	}

	// MobDropItem Group ("kill count" pool): every entry in the pool is a
	// possible pick, not just the one GetOne() would roll, once the pool's
	// own overall chance (kills-per-drop, scaled the same way
	// CreateDropItem does) clears 1.
	{
		auto it = m_map_pkMobItemGroup.find(race);
		if (it != m_map_pkMobItemGroup.end())
		{
			CMobItemGroup * pGroup = it->second;
			if (pGroup && !pGroup->IsEmpty())
			{
				int iPercent = 40000 * iDeltaPercent / pGroup->GetKillPerDrop();
				if (iPercent >= 1)
				{
					const std::vector<CMobItemGroup::SMobItemGroupInfo> & vec = pGroup->GetItems();
					for (size_t i = 0; i < vec.size(); ++i)
						out_vnums.insert(vec[i].dwItemVnum);
				}
			}
		}
	}

	// Level Item Group: gated by the killer's own level against the group's
	// limit, exactly as CreateDropItem checks it.
	{
		auto it = m_map_pkLevelItemGroup.find(race);
		if (it != m_map_pkLevelItemGroup.end())
		{
			if (it->second->GetLevelLimit() <= (DWORD) iLevel)
			{
				const auto & vec = it->second->GetVector();
				for (size_t i = 0; i < vec.size(); ++i)
					if (vec[i].dwPct >= 1)
						out_vnums.insert(vec[i].dwVNum);
			}
		}
	}

	// Thief-gloves / premium-item bonus group: only reachable at all with
	// that equipment or that premium active, exactly like CreateDropItem.
	{
		if (pkKiller->GetPremiumRemainSeconds(PREMIUM_ITEM) > 0 ||
				pkKiller->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM))
		{
			auto it = m_map_pkGloveItemGroup.find(race);
			if (it != m_map_pkGloveItemGroup.end())
			{
				const auto & vec = it->second->GetVector();
				for (size_t i = 0; i < vec.size(); ++i)
				{
					int iPercent = (vec[i].dwPct * iDeltaPercent) / 100;
					if (iPercent >= iPreviewMinPercent)
						out_vnums.insert(vec[i].dwVnum);
				}
			}
		}
	}

	// Etc drop: the one item tied to this specific mob instance/proto's own
	// drop-item vnum (a chest key, typically).
	if (pkChr->GetMobDropItemVnum())
	{
		auto it = m_map_dwEtcItemDropProb.find(pkChr->GetMobDropItemVnum());
		if (it != m_map_dwEtcItemDropProb.end())
		{
			int iPercent = (it->second * iDeltaPercent) / 100;
			if (iPercent >= iPreviewMinPercent)
				out_vnums.insert(pkChr->GetMobDropItemVnum());
		}
	}

	// Metin stone's own bonus stone, and the guaranteed one skill book a
	// stone always tops up to (never added on top) for a killer within
	// PLAYERBOT_METIN_BOOK_LEVEL_DELTA levels of it -- see CreateDropItem's
	// own comment on both.
	if (pkChr->IsStone())
	{
		if (pkChr->GetDropMetinStoneVnum())
		{
			int iPercent = (pkChr->GetDropMetinStonePct() * iDeltaPercent) * 400;
			if (iPercent >= 1)
				out_vnums.insert(pkChr->GetDropMetinStoneVnum());
		}

		if (pkKiller->GetLevel() <= pkChr->GetLevel() + PLAYERBOT_METIN_BOOK_LEVEL_DELTA)
			out_vnums.insert(50300);
	}

	// Moonlight Treasure Chest and the Dragon Coin voucher: CONFIG permille,
	// per kill/stone/boss -- what CreateDropItem itself reads, not a roll.
	{
		int chestPermille = pkChr->IsStone() ? g_iMoonlightChestStonePermille : g_iMoonlightChestPermille;
		if (chestPermille > 0)
			out_vnums.insert(50011);

		int coinPermille = pkChr->IsStone()
				? g_iDragonCoinStonePermille
				: (pkChr->GetMobRank() >= MOB_RANK_BOSS ? g_iDragonCoinBossPermille : 0);
		if (coinPermille > 0)
			out_vnums.insert(80017);
	}

	// Horse skill book: needs the killer mounted, same as CreateDropItem,
	// plus the event flag GetDropPerKillPct folds in.
	if (pkKiller->IsHorseRiding() && GetDropPerKillPct(1000, 1000000, iDeltaPercent, "horse_skill_book_drop") >= 1)
		out_vnums.insert(ITEM_HORSE_SKILL_TRAIN_BOOK);

	// ---- CreateQuestDropItem's own event-gated branches -------------------
	// Every one of these is gated by quest::GetEventFlag() (a read, not a
	// roll) and, where GetDropPerKillPct() is involved, the same read
	// wrapped the same way GetDropPct() already is above -- see that
	// function's own comment for why 0 there always means "off".
	int deltaLevel = pkKiller->GetLevel() - pkChr->GetLevel();

	if (quest::CQuestManager::instance().GetEventFlag("xmas_sock"))
		out_vnums.insert(50010);

	if (quest::CQuestManager::instance().GetEventFlag("drop_moon"))
		out_vnums.insert(50011);

	if (pkKiller->GetLevel() >= 15 && abs(deltaLevel) <= 5 &&
			quest::CQuestManager::instance().GetEventFlag("hc_drop") > 0)
		out_vnums.insert(30178);

	if (quest::CQuestManager::instance().GetEventFlag("valentine_event") &&
			pkKiller->GetLevel() >= 20 && deltaLevel < 10 &&
			MIN(quest::CQuestManager::instance().GetEventFlag("valentine_drop"), 500) > 0)
	{
		out_vnums.insert(50024);
		out_vnums.insert(50025);
	}

	if (quest::CQuestManager::instance().GetEventFlag("hex_drop") &&
			pkKiller->GetLevel() >= 20 && deltaLevel < 10 &&
			MIN(quest::CQuestManager::instance().GetEventFlag("hex_drop"), 500) > 0)
		out_vnums.insert(50037);

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "2007_drop") >= 1)
		out_vnums.insert(50043);

	if (GetDropPerKillPct(100, 1000, iDeltaPercent, "newyear_fire") >= 1)
		out_vnums.insert(50107);

	if (GetDropPerKillPct(100, 500, iDeltaPercent, "newyear_moon") >= 1)
	{
		out_vnums.insert(50016);
		out_vnums.insert(50017);
		out_vnums.insert(50018);
		out_vnums.insert(50019);
	}

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "icecream_drop") >= 1)
		out_vnums.insert(50123);

	// new_xmas_event tops up an item the killer already holds via
	// AutoGiveItem, not CreateItem -- still a possible drop, no item lookup
	// needed since 50010 is a fixed literal both branches share.
	if ((pkKiller->CountSpecifyItem(53002) > 0 || pkKiller->CountSpecifyItem(53007) > 0) &&
			GetDropPerKillPct(50, 100, iDeltaPercent, "new_xmas_event") >= 1)
		out_vnums.insert(50010);

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "halloween_drop") >= 1)
		out_vnums.insert(30321);

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "ramadan_drop") >= 1)
		out_vnums.insert(30315);

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "easter_drop") >= 1)
	{
		for (DWORD v = 50160; v <= 50179; ++v)
			out_vnums.insert(v);
	}

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "football_drop") >= 1)
		out_vnums.insert(50096);

	if (GetDropPerKillPct(100, 2000, iDeltaPercent, "whiteday_drop") >= 1)
	{
		out_vnums.insert(ITEM_WHITEDAY_ROSE);
		out_vnums.insert(ITEM_WHITEDAY_CANDY);
	}

	if (pkKiller->GetLevel() >= 50)
	{
		if (GetDropPerKillPct(100, 1000, iDeltaPercent, "kids_day_drop_high") >= 1)
			out_vnums.insert(50034);
	}
	else
	{
		if (GetDropPerKillPct(100, 1000, iDeltaPercent, "kids_day_drop") >= 1)
			out_vnums.insert(50034);
	}

	if (pkChr->GetLevel() >= 30 && GetDropPerKillPct(50, 100, iDeltaPercent, "medal_part_drop") >= 1)
	{
		static const DWORD medal_parts[] = { 30265, 30266, 30267, 30268, 30269 };
		for (size_t i = 0; i < _countof(medal_parts); ++i)
			out_vnums.insert(medal_parts[i]);
	}

	if (pkChr->GetLevel() >= 40 && pkChr->GetMobRank() >= MOB_RANK_BOSS &&
			GetDropPerKillPct(1, 1000, iDeltaPercent, "three_skill_item") / GetThreeSkillLevelAdjust(pkChr->GetLevel()) >= 1)
		out_vnums.insert(50513);

	if (GetDropPerKillPct(100, 1000, iDeltaPercent, "dragon_boat_festival_drop") >= 1)
		out_vnums.insert(50085);

	if (pkKiller->GetLevel() >= 15 && quest::CQuestManager::instance().GetEventFlag("mars_drop"))
	{
		static const int iDropMultiply[MOB_RANK_MAX_NUM] = { 50, 30, 5, 1, 0, 0 };
		if (iDropMultiply[pkChr->GetMobRank()] &&
				GetDropPerKillPct(1000, 1500, iDeltaPercent, "mars_drop") >= 1)
			out_vnums.insert(70035);
	}

	// ---- The two admin-toggled "drop events" (DropEvent_CharStone_SetValue
	// / DropEvent_RefineBox_SetValue) -- gs_dropEvent_charStone/refineBox are
	// file-static in this translation unit, which is exactly why this
	// function lives here rather than in char_item.cpp alongside the chat
	// command that calls it.
	if (pkChr->IsStone() && gs_dropEvent_charStone.alive)
	{
		int killer_level = pkKiller->GetLevel();
		int victim_level = pkChr->GetLevel();
		int level_diff = victim_level - killer_level;
		if (level_diff < gs_dropEvent_charStone.level_range && level_diff > -gs_dropEvent_charStone.level_range)
		{
			static const DWORD Stones[] = { 30210, 30211, 30212, 30213, 30214, 30215, 30216, 30217, 30218, 30219, 30258, 30259, 30260, 30261, 30262, 30263 };
			for (size_t i = 0; i < _countof(Stones); ++i)
				out_vnums.insert(Stones[i]);
		}
	}

	if (gs_dropEvent_refineBox.alive && pkChr->GetMobRank() >= MOB_RANK_KNIGHT)
	{
		int killer_level = pkKiller->GetLevel();
		static const DWORD lowerBox[] = { 50197, 50198, 50199 };
		static const DWORD midderBox[] = { 50203, 50204, 50205, 50206 };
		static const DWORD higherBox[] = { 50207, 50208, 50209, 50210, 50211 };
		const DWORD * box; size_t count;
		if (killer_level <= gs_dropEvent_refineBox.low)
			{ box = lowerBox; count = _countof(lowerBox); }
		else if (killer_level <= gs_dropEvent_refineBox.mid)
			{ box = midderBox; count = _countof(midderBox); }
		else
			{ box = higherBox; count = _countof(higherBox); }
		for (size_t i = 0; i < count; ++i)
			out_vnums.insert(box[i]);
	}

	// Filter out anything that resolved to a vnum with no prototype, so a
	// stale table entry never reaches the client as a phantom icon.
	for (auto it = out_vnums.begin(); it != out_vnums.end(); )
	{
		if (!GetTable(*it))
			it = out_vnums.erase(it);
		else
			++it;
	}

	if (out_vnums.size() > 1024)
	{
		auto it = out_vnums.begin();
		std::advance(it, 1024);
		out_vnums.erase(it, out_vnums.end());
		out_complete = false;
	}
}

// What a monster or a Metin stone can drop, from the tables this process
// loaded (/mob_drop_preview in cmd_general.cpp, client-root/uimobpreview.py).
// Everything CreateDropItem above rolls for the race is listed, the
// playerbot additions under their own conditions, for this viewer: its level
// and its gloves decide as a killer's would. A quest's drop depends on the
// quest, and the common drop by level and the horse's skill book on the
// killer's level and saddle, so none of those is listed. An item is one line
// with its biggest count however many tables name it - Metin Szeptow (8028)
// names its weapons and armour three times - and a special group is expanded
// once.
static const int PLAYERBOT_MOB_PREVIEW_MAX_LINES = 400;
static const int PLAYERBOT_MOB_PREVIEW_MAX_DEPTH = 4;
// Pirate Tanaka's fall drops nothing from the tables (CHARACTER::Reward
// returns before CreateDropItem for him): his yang and, for the one who hurt
// him most, his ear (the Tanaka event, playerbotify apply_tanaka_goblin).
static const DWORD PLAYERBOT_MOB_PREVIEW_TANAKA_VNUM = 5001;
static const DWORD PLAYERBOT_MOB_PREVIEW_TANAKA_EAR_VNUM = 30202;

static void AddMobPreviewItem(std::vector<std::pair<DWORD, int> >& lines, DWORD vnum, int amount)
{
	// The tables' own Moonlight chests drop only while a chest window is open.
	if (vnum == 50011 && g_iMoonlightChestPermille <= 0 && g_iMoonlightChestStonePermille <= 0)
		return;
	if (amount <= 0 || !ITEM_MANAGER::instance().GetTable(vnum))
		return;
	for (auto& line : lines)
		if (line.first == vnum)
		{
			line.second = MAX(line.second, amount);
			return;
		}
	if ((int) lines.size() < PLAYERBOT_MOB_PREVIEW_MAX_LINES)
		lines.emplace_back(vnum, amount);
}

static void AddMobPreviewSpecialGroup(std::vector<std::pair<DWORD, int> >& lines, DWORD groupVnum, std::set<DWORD>& seen, int depth)
{
	if (depth > PLAYERBOT_MOB_PREVIEW_MAX_DEPTH || !seen.insert(groupVnum).second)
		return;
	const CSpecialItemGroup* group = ITEM_MANAGER::instance().GetSpecialItemGroup(groupVnum);
	if (!group)
		return;
	for (const auto& reward : group->GetItems())
	{
		if (reward.isSpecial)
			AddMobPreviewSpecialGroup(lines, reward.vnum, seen, depth + 1);
		// A drop is an item or nothing (GetSpecialItemGroupDropInfo): yang and
		// the other effects of a group are a chest's, not a monster's.
		else if (reward.vnum > CSpecialItemGroup::MOB_GROUP)
			AddMobPreviewItem(lines, reward.vnum, reward.count);
	}
}

// The spirit stones CHARACTER::DetermineDropMetinStone draws from, the same
// list under the same switches (its own is a local of that function), so the
// preview can name every one of them (apply_mob_preview_stone_kinds).
static const DWORD PLAYERBOT_MOB_PREVIEW_METIN_STONES[] =
{
#if defined(ENABLE_WOLFMAN_CHARACTER) && defined(USE_WOLFMAN_STONES)
	28012,
#endif
	28030, 28031, 28032, 28033, 28034, 28035, 28036,
	28037, 28038, 28039, 28040, 28041, 28042, 28043,
#if defined(ENABLE_MAGIC_REDUCTION_SYSTEM) && defined(USE_MAGIC_REDUCTION_STONES)
	28044, 28045,
#endif
};

// Every kind and grade the stone a Metin carries can be. The grade is drawn
// as DetermineDropMetinStone draws it: the first STONE_LEVEL_MAX_NUM portions
// of its aStoneDrop row in turn, and the top grade whatever they leave of a
// hundred; a portion nothing can reach is no grade.
static void AddMobPreviewMetinStones(std::vector<std::pair<DWORD, int> >& lines, DWORD race)
{
#ifdef ENABLE_NEWSTUFF
	if (g_NoDropMetinStone)
		return;
#endif
	const int idx = std::lower_bound(aStoneDrop, aStoneDrop + STONE_INFO_MAX_NUM, race) - aStoneDrop;
	if (idx >= STONE_INFO_MAX_NUM || aStoneDrop[idx].dwMobVnum != race || aStoneDrop[idx].iDropPct <= 0)
		return;
	const SStoneDropInfo& info = aStoneDrop[idx];
	bool grades[STONE_LEVEL_MAX_NUM + 1] = {};
	int used = 0;
	for (int grade = 0; grade < STONE_LEVEL_MAX_NUM; ++grade)
	{
		const int portion = MIN(info.iLevelPct[grade], 100 - used);
		grades[grade] = portion > 0;
		used += MAX(0, portion);
	}
	grades[STONE_LEVEL_MAX_NUM] = used < 100;
	for (DWORD stone : PLAYERBOT_MOB_PREVIEW_METIN_STONES)
		for (int grade = 0; grade <= STONE_LEVEL_MAX_NUM; ++grade)
			if (grades[grade])
				AddMobPreviewItem(lines, stone + 100 * grade, 1);
}

void ITEM_MANAGER::SendMobDropPreview(LPCHARACTER viewer, LPCHARACTER mob)
{
	if (!viewer || !mob)
		return;
	const DWORD race = mob->GetRaceNum();
	std::vector<std::pair<DWORD, int> > lines;
	if (race == PLAYERBOT_MOB_PREVIEW_TANAKA_VNUM)
		AddMobPreviewItem(lines, PLAYERBOT_MOB_PREVIEW_TANAKA_EAR_VNUM, 1);
	else
	{
		std::set<DWORD> seen;
		auto drop = m_map_pkDropItemGroup.find(race);
		if (drop != m_map_pkDropItemGroup.end())
			for (const auto& reward : drop->second->GetVector())
			{
				if (reward.bSpecialGroup)
					AddMobPreviewSpecialGroup(lines, reward.dwVnum, seen, 0);
				else
					AddMobPreviewItem(lines, reward.dwVnum, reward.iCount);
			}

		auto kill = m_map_pkMobItemGroup.find(race);
		if (kill != m_map_pkMobItemGroup.end() && kill->second && !kill->second->IsEmpty())
			for (const auto& reward : kill->second->GetItems())
				AddMobPreviewItem(lines, reward.dwItemVnum, reward.iCount);

		auto level = m_map_pkLevelItemGroup.find(race);
		if (level != m_map_pkLevelItemGroup.end() && level->second->GetLevelLimit() <= (DWORD) viewer->GetLevel())
			for (const auto& reward : level->second->GetVector())
				AddMobPreviewItem(lines, reward.dwVNum, reward.iCount);

		if (viewer->GetPremiumRemainSeconds(PREMIUM_ITEM) > 0 || viewer->IsEquipUniqueGroup(UNIQUE_GROUP_DOUBLE_ITEM))
		{
			auto glove = m_map_pkGloveItemGroup.find(race);
			if (glove != m_map_pkGloveItemGroup.end())
				for (const auto& reward : glove->second->GetVector())
					AddMobPreviewItem(lines, reward.dwVnum, reward.iCount);
		}

		if (mob->GetMobDropItemVnum() && m_map_dwEtcItemDropProb.count(mob->GetMobDropItemVnum()))
			AddMobPreviewItem(lines, mob->GetMobDropItemVnum(), 1);

		if (mob->IsStone())
		{
			// What the stone can hold, not what this one holds
			// (apply_mob_preview_stone_kinds).
			AddMobPreviewMetinStones(lines, race);
			// MT2009_PLUS_DUNGEON_DROP_V1 (preview book): no book top-up from the dungeons' Metins.
			if (viewer->GetLevel() <= mob->GetLevel() + PLAYERBOT_METIN_BOOK_LEVEL_DELTA && !Mt2009PlusDungeonMob(race))
			{
				AddMobPreviewItem(lines, 50300, 1);
				if (g_iBlessingScrollStonePermille > 0 &&
						mob->GetLevel() >= PLAYERBOT_BLESSING_SCROLL_STONE_MIN_LEVEL &&
						mob->GetLevel() <= PLAYERBOT_BLESSING_SCROLL_STONE_MAX_LEVEL)
					AddMobPreviewItem(lines, 25040, 1);
			}
		}

		// MT2009_PLUS_DUNGEON_DROP_V1 (preview): none of the world's extras for the Razador and Nemere
		// dungeons' own monsters (CreateDropItem above).
		if (!Mt2009PlusDungeonMob(race))
		{
		if ((mob->IsStone() ? g_iMoonlightChestStonePermille : g_iMoonlightChestPermille) > 0)
			AddMobPreviewItem(lines, 50011, 1);
		if ((mob->IsStone() ? g_iDragonCoinStonePermille
				: (mob->GetMobRank() >= MOB_RANK_BOSS ? g_iDragonCoinBossPermille : 0)) > 0)
			AddMobPreviewItem(lines, 80017, 1);
		// MT2009_PLUS_PREVIEW_RARE_V1: the Cor Draconis and the sash of a Metin
		// or a boss at most 15 levels under the viewer, while the world has
		// them on (CreateDropItem above; server-patches/botraredrop, rarelevel,
		// raretoggle). The engine counts the Metin stones as bosses too.
		if (mob->GetMobRank() >= MOB_RANK_BOSS && viewer->GetLevel() <= mob->GetLevel() + 15)
		{
			if (quest::CQuestManager::instance().GetEventFlag("m2_alchemy_off") == 0)
				AddMobPreviewItem(lines, 50255, 1);
			if (quest::CQuestManager::instance().GetEventFlag("m2_sash_off") == 0)
			{
				static const DWORD s_aPreviewSash[] = { 85001, 85005, 85011, 85015, 85021 };
				for (DWORD sash : s_aPreviewSash)
					AddMobPreviewItem(lines, sash, 1);
			}
		}
		}
	}

	viewer->ChatPacket(CHAT_TYPE_COMMAND, "MobPreviewBegin %u", race);
	for (const auto& line : lines)
		viewer->ChatPacket(CHAT_TYPE_COMMAND, "MobPreviewItem %u|%d", line.first, line.second);
	viewer->ChatPacket(CHAT_TYPE_COMMAND, "MobPreviewEnd %u", race);
}

// MT2009_PLUS_DROP_WIKI_V1: the drop wiki (client root uidropwiki.py, /drop_wiki
// in cmd_general.cpp) - what a race drops and how often, from the tables this
// process loaded, for a killer at the monster's own level with the server's mob
// item rate and nothing more: no premium, thief gloves, events or party. Read as
// CreateDropItem above reads it, the way SendMobDropPreview does: the race's
// tables (drop, kill, limit), the etc drop, a Metin's spirit stones and the
// world's extras (skill book, Blessing Scroll, Moonlight chest, Dragon Coin
// voucher, Cor Draconis, sash). Not listed: the common drop (it goes by the
// killer's level, not the monster), the thief-gloves table, quest drops, the
// horse book, the double-loot events and SeonHae. The same item from several
// draws is one row: the chance that at least one of them drops it, and the
// smallest and biggest count.
static const size_t MT2009_PLUS_DROP_WIKI_MAX_ROWS = 400;

static void DropWikiAdd(std::vector<TDropWikiRow>& rows, DWORD vnum, int count, double chance)
{
	if (vnum <= CSpecialItemGroup::MOB_GROUP || count <= 0 || chance <= 0.0)
		return;
	if (!ITEM_MANAGER::instance().GetTable(vnum))
		return;
	if (chance > 1.0)
		chance = 1.0;
	for (auto& row : rows)
		if (row.dwVnum == vnum)
		{
			row.dChance = 1.0 - (1.0 - row.dChance) * (1.0 - chance);
			row.iCountMin = MIN(row.iCountMin, count);
			row.iCountMax = MAX(row.iCountMax, count);
			return;
		}
	if (rows.size() < MT2009_PLUS_DROP_WIKI_MAX_ROWS)
	{
		TDropWikiRow row;
		row.dwVnum = vnum;
		row.iCountMin = count;
		row.iCountMax = count;
		row.dChance = chance;
		rows.push_back(row);
	}
}

// A special group in a drop table gives one of its entries by weight
// (GetSpecialItemGroupDropInfo); yang and the other effects give nothing.
static void DropWikiAddSpecialGroup(std::vector<TDropWikiRow>& rows, DWORD groupVnum, double chance, int depth)
{
	if (depth > PLAYERBOT_MOB_PREVIEW_MAX_DEPTH || chance <= 0.0)
		return;
	const CSpecialItemGroup* group = ITEM_MANAGER::instance().GetSpecialItemGroup(groupVnum);
	if (!group || group->IsEmpty())
		return;
	const std::vector<CSpecialItemGroup::CSpecialItemInfo> items = group->GetItems();
	const std::vector<int> probs = group->GetProbs();
	const double total = (double) probs.back();
	if (total <= 0.0)
		return;
	for (size_t i = 0; i < items.size() && i < probs.size(); ++i)
	{
		const double share = (double) (probs[i] - (i ? probs[i - 1] : 0)) / total;
		if (items[i].isSpecial)
			DropWikiAddSpecialGroup(rows, items[i].vnum, chance * share, depth + 1);
		else
			DropWikiAdd(rows, items[i].vnum, items[i].count, chance * share);
	}
}

// A Metin's spirit stone: one kind of PLAYERBOT_MOB_PREVIEW_METIN_STONES at
// random, its grade drawn as CHARACTER::DetermineDropMetinStone draws it, and
// dropped at the stone's own chance (CreateDropItem).
static void DropWikiAddMetinStones(std::vector<TDropWikiRow>& rows, DWORD race, int iDeltaPercent, double range)
{
#ifdef ENABLE_NEWSTUFF
	if (g_NoDropMetinStone)
		return;
#endif
	const int idx = std::lower_bound(aStoneDrop, aStoneDrop + STONE_INFO_MAX_NUM, race) - aStoneDrop;
	if (idx >= STONE_INFO_MAX_NUM || aStoneDrop[idx].dwMobVnum != race || aStoneDrop[idx].iDropPct <= 0)
		return;
	const SStoneDropInfo& info = aStoneDrop[idx];
	const double dropChance = (double) info.iDropPct * iDeltaPercent * 400.0 / range;
	const double kinds = (double) _countof(PLAYERBOT_MOB_PREVIEW_METIN_STONES);
	int left = 100;
	for (int grade = 0; grade <= STONE_LEVEL_MAX_NUM; ++grade)
	{
		int portion = left;
		if (grade < STONE_LEVEL_MAX_NUM)
		{
			portion = MIN(MAX(0, info.iLevelPct[grade]), left);
			left -= portion;
		}
		if (portion <= 0)
			continue;
		for (DWORD stone : PLAYERBOT_MOB_PREVIEW_METIN_STONES)
			DropWikiAdd(rows, stone + 100 * grade, 1, dropChance / kinds * portion / 100.0);
	}
}

void ITEM_MANAGER::GetDropWikiRows(DWORD race, std::vector<TDropWikiRow>& rows)
{
	rows.clear();
	const CMob* mob = CMobManager::instance().Get(race);
	if (!mob)
		return;
	const TMobTable& table = mob->m_table;
	const bool stone = table.bType == CHAR_TYPE_STONE;
	if (table.bType != CHAR_TYPE_MONSTER && !stone)
		return;
	// Razador and Nemere drop only yang (CreateDropItem), Tanaka only his ear
	// (SendMobDropPreview).
	if (race == 6091 || race == 6191)
		return;
	if (race == PLAYERBOT_MOB_PREVIEW_TANAKA_VNUM)
	{
		DropWikiAdd(rows, PLAYERBOT_MOB_PREVIEW_TANAKA_EAR_VNUM, 1, 1.0);
		return;
	}

	// GetDropPct for a killer of the monster's own level.
	const int level = table.bLevel;
	const bool big = stone || table.bRank >= MOB_RANK_BOSS;
	int iDeltaPercent = big ? PERCENT_LVDELTA_BOSS(level, level) : PERCENT_LVDELTA(level, level);
	const int itemRate = g_bChinaIntoxicationCheck ? 100 : CHARACTER_MANAGER::instance().GetMobItemRate(NULL);
	iDeltaPercent = iDeltaPercent * itemRate / 100;
	const double range = distribution_test_server ? (double) (4000000 / 3) : 4000000.0;

	// Drop Item Group
	auto drop = m_map_pkDropItemGroup.find(race);
	if (drop != m_map_pkDropItemGroup.end())
		for (const auto& info : drop->second->GetVector())
		{
			const double chance = (double) info.dwPct * iDeltaPercent / 100.0 / range;
			if (info.bSpecialGroup)
				DropWikiAddSpecialGroup(rows, info.dwVnum, chance, 0);
			else
				DropWikiAdd(rows, info.dwVnum, info.iCount, chance);
		}

	// MobDropItem Group: the pool drops at its kills-per-drop, then one entry by weight.
	auto kill = m_map_pkMobItemGroup.find(race);
	if (kill != m_map_pkMobItemGroup.end() && kill->second && !kill->second->IsEmpty() && kill->second->GetKillPerDrop() > 0)
	{
		const double poolChance = (double) (40000 * iDeltaPercent / kill->second->GetKillPerDrop()) / range;
		const std::vector<CMobItemGroup::SMobItemGroupInfo> items = kill->second->GetItems();
		const std::vector<int> probs = kill->second->GetProbs();
		const double total = (double) probs.back();
		if (total > 0.0)
			for (size_t i = 0; i < items.size() && i < probs.size(); ++i)
				DropWikiAdd(rows, items[i].dwItemVnum, items[i].iCount,
					poolChance * (double) (probs[i] - (i ? probs[i - 1] : 0)) / total);
	}

	// Level Item Group: a flat chance per million for a killer at its level limit or over.
	auto limit = m_map_pkLevelItemGroup.find(race);
	if (limit != m_map_pkLevelItemGroup.end())
		for (const auto& info : limit->second->GetVector())
			DropWikiAdd(rows, info.dwVNum, info.iCount, (double) info.dwPct / 1000000.0);

	// The etc drop of the monster's proto.
	if (table.dwDropItemVnum)
	{
		auto etc = m_map_dwEtcItemDropProb.find(table.dwDropItemVnum);
		if (etc != m_map_dwEtcItemDropProb.end())
			DropWikiAdd(rows, table.dwDropItemVnum, 1, (double) etc->second * iDeltaPercent / 100.0 / range);
	}

	if (stone)
		DropWikiAddMetinStones(rows, race, iDeltaPercent, range);

	// The world's extras, none of them for the Razador and Nemere dungeons' own monsters.
	if (!Mt2009PlusDungeonMob(race))
	{
		if (stone)
		{
			// The guaranteed book (topped up to one).
			DropWikiAdd(rows, 50300, 1, 1.0);
			if (g_iBlessingScrollStonePermille > 0 &&
					level >= PLAYERBOT_BLESSING_SCROLL_STONE_MIN_LEVEL &&
					level <= PLAYERBOT_BLESSING_SCROLL_STONE_MAX_LEVEL)
				DropWikiAdd(rows, 25040, 1, g_iBlessingScrollStonePermille / 1000.0);
		}

		const int chestPermille = stone ? g_iMoonlightChestStonePermille : g_iMoonlightChestPermille;
		if (chestPermille > 0)
			DropWikiAdd(rows, 50011, 1, chestPermille / 1000.0);

		const int coinPermille = stone ? g_iDragonCoinStonePermille
				: (table.bRank >= MOB_RANK_BOSS ? g_iDragonCoinBossPermille : 0);
		if (coinPermille > 0)
			DropWikiAdd(rows, 80017, 1, coinPermille / 1000.0);

		// Only the rules for any map: a map's own rule is that map's.
		const Mt2009PlusRareMobRule* rule = Mt2009PlusRareMobRuleForRace(race, -1);
		if (quest::CQuestManager::instance().GetEventFlag("m2_alchemy_off") == 0)
		{
			int corChance = stone ? 20 : (table.bRank >= MOB_RANK_BOSS ? 80 : 0);
			if (rule)
				corChance = rule->corChance;
			if (corChance > 0)
				DropWikiAdd(rows, 50255, rule && rule->corCount > 1 ? rule->corCount : 1, corChance / 100.0);
		}
		if (table.bRank >= MOB_RANK_BOSS && quest::CQuestManager::instance().GetEventFlag("m2_sash_off") == 0)
		{
			static const DWORD s_aWikiSash[] = { 85001, 85005, 85011, 85015, 85021 };
			int sashChance = stone ? 15 : 80;
			if (rule)
				sashChance = rule->sashChance;
			if (sashChance > 0)
				for (DWORD sash : s_aWikiSash)
					DropWikiAdd(rows, sash, 1, sashChance / 100.0 / _countof(s_aWikiSash));
		}

		// The tables' own Moonlight chests drop only while a chest window is open.
		if (g_iMoonlightChestPermille <= 0 && g_iMoonlightChestStonePermille <= 0)
			for (size_t i = 0; i < rows.size();)
			{
				if (rows[i].dwVnum == 50011)
					rows.erase(rows.begin() + i);
				else
					++i;
			}
	}

	std::sort(rows.begin(), rows.end(), [](const TDropWikiRow& a, const TDropWikiRow& b)
	{
		if (a.dChance != b.dChance)
			return a.dChance > b.dChance;
		return a.dwVnum < b.dwVnum;
	});
}

DWORD ITEM_MANAGER::GetRefineFromVnum(DWORD dwVnum)
{
	itertype(m_map_ItemRefineFrom) it = m_map_ItemRefineFrom.find(dwVnum);
	if (it != m_map_ItemRefineFrom.end())
		return it->second;
	return 0;
}

const CSpecialItemGroup* ITEM_MANAGER::GetSpecialItemGroup(DWORD dwVnum)
{
	itertype(m_map_pkSpecialItemGroup) it = m_map_pkSpecialItemGroup.find(dwVnum);
	if (it != m_map_pkSpecialItemGroup.end())
	{
		return it->second;
	}
	return NULL;
}

const TSpecialItemGroupDropInfo ITEM_MANAGER::GetSpecialItemGroupDropInfo(DWORD dwVnum)
{
	const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwVnum);
	while (pGroup != NULL)
	{
		int idx = pGroup->GetOneIndex();
		bool isSpecial = pGroup->IsSpecial(idx);
		if (isSpecial)
		{
			pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(pGroup->GetVnum(idx));
			continue;
		}

		TSpecialItemGroupDropInfo info{};
		info.dwVnum = pGroup->GetVnum(idx);
		info.dwCount = pGroup->GetCount(idx);
		info.iRarePct = pGroup->GetRarePct(idx);
		return info;
	}
	return {};
}

const CSpecialAttrGroup* ITEM_MANAGER::GetSpecialAttrGroup(DWORD dwVnum)
{
	itertype(m_map_pkSpecialAttrGroup) it = m_map_pkSpecialAttrGroup.find(dwVnum);
	if (it != m_map_pkSpecialAttrGroup.end())
	{
		return it->second;
	}
	return NULL;
}

DWORD ITEM_MANAGER::GetMaskVnum(DWORD dwVnum)
{
	TMapDW2DW::iterator it = m_map_new_to_ori.find (dwVnum);
	if (it != m_map_new_to_ori.end())
	{
		return it->second;
	}
	else
		return 0;
}

void ITEM_MANAGER::CopyAllAttrTo(LPITEM pkOldItem, LPITEM pkNewItem)
{
	// ACCESSORY_REFINE
	if (pkOldItem->IsAccessoryForSocket())
	{
		for (int i = 0; i < METIN_SOCKET_MAX_NUM; ++i)
		{
			pkNewItem->SetSocket(i, pkOldItem->GetSocket(i));
		}
		//pkNewItem->StartAccessorySocketExpireEvent();
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		for (int i = 0; i < METIN_SOCKET_MAX_NUM; ++i)
		{
			if (!pkOldItem->GetSocket(i))
				break;
			else
				pkNewItem->SetSocket(i, 1);
		}

		int slot = 0;

		for (int i = 0; i < METIN_SOCKET_MAX_NUM; ++i)
		{
			long socket = pkOldItem->GetSocket(i);
			const int ITEM_BROKEN_METIN_VNUM = 28960;
			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				pkNewItem->SetSocket(slot++, socket);
		}

	}

	for (int i = METIN_SOCKET_MAX_NUM; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		auto oldSocket = pkOldItem->GetSocket(i);
		pkNewItem->SetSocket(i++, oldSocket);
	}

	pkOldItem->CopyAttributeTo(pkNewItem);
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top

