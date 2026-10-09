#include "stdafx.h"
#include "char.h"
#include "item.h"
#include "desc.h"
#include "DragonSoul.h"
#include "log.h"

void CHARACTER::DragonSoul_Initialize()
{
	for (int i = INVENTORY_MAX_NUM + WEAR_MAX_NUM; i < DRAGON_SOUL_EQUIP_SLOT_END; i++)
	{
		LPITEM pItem = GetItem(TItemPos(INVENTORY, i));
		if (NULL != pItem)
			pItem->SetSocket(ITEM_SOCKET_DRAGON_SOUL_ACTIVE_IDX, 0);
	}

	if (FindAffect(AFFECT_DRAGON_SOUL_DECK_0))
	{
		DragonSoul_ActivateDeck(DRAGON_SOUL_DECK_0);
	}
	else if (FindAffect(AFFECT_DRAGON_SOUL_DECK_1))
	{
		DragonSoul_ActivateDeck(DRAGON_SOUL_DECK_1);
	}
}

int CHARACTER::DragonSoul_GetActiveDeck() const
{
	return m_pointsInstant.iDragonSoulActiveDeck;
}

bool CHARACTER::DragonSoul_IsDeckActivated() const
{
	return m_pointsInstant.iDragonSoulActiveDeck >= 0;
}

bool CHARACTER::DragonSoul_IsQualified() const
{
	#ifdef ENABLE_NO_DSS_QUALIFICATION
	return true;
	#else
	return FindAffect(AFFECT_DRAGON_SOUL_QUALIFIED) != NULL;
	#endif
}

void CHARACTER::DragonSoul_GiveQualification()
{
	if(NULL == FindAffect(AFFECT_DRAGON_SOUL_QUALIFIED))
	{
		LogManager::instance().CharLog(this, 0, "DS_QUALIFIED", "");
	}
	AddAffect(AFFECT_DRAGON_SOUL_QUALIFIED, POINT_NONE, 0, AFF_NONE, INFINITE_AFFECT_DURATION, 0, false, false);
	//SetQuestFlag("dragon_soul.is_qualified", 1);
	//PointChange(POINT_DRAGON_SOUL_IS_QUALIFIED, 1 - GetPoint(POINT_DRAGON_SOUL_IS_QUALIFIED));
}

bool CHARACTER::DragonSoul_ActivateDeck(int deck_idx)
{
	if (deck_idx < DRAGON_SOUL_DECK_0 || deck_idx >= DRAGON_SOUL_DECK_MAX_NUM)
	{
		return false;
	}

	if (DragonSoul_GetActiveDeck() == deck_idx)
		return true;

	DragonSoul_DeactivateAll();

	if (!DragonSoul_IsQualified())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The Dragon Soul Stone box is not activated."));
		return false;
	}

	AddAffect(AFFECT_DRAGON_SOUL_DECK_0 + deck_idx, POINT_NONE, 0, 0, INFINITE_AFFECT_DURATION, 0, false);

	m_pointsInstant.iDragonSoulActiveDeck = deck_idx;

	for (int i = DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * deck_idx;
		i < DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * (deck_idx + 1); i++)
	{
		LPITEM pItem = GetInventoryItem(i);
		if (NULL != pItem)
			DSManager::instance().ActivateDragonSoul(pItem);
	}
#ifdef ENABLE_DS_SET
	DragonSoul_SetBonus();
#endif
	return true;
}

void CHARACTER::DragonSoul_DeactivateAll()
{
	for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; i++)
	{
		DSManager::instance().DeactivateDragonSoul(GetInventoryItem(i), true);
	}
	m_pointsInstant.iDragonSoulActiveDeck = -1;
	RemoveAffect(AFFECT_DRAGON_SOUL_DECK_0);
	RemoveAffect(AFFECT_DRAGON_SOUL_DECK_1);
#ifdef ENABLE_DS_SET
	RemoveAffect(AFFECT_DS_SET);
#endif
}

void CHARACTER::DragonSoul_CleanUp()
{
	for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; i++)
	{
		DSManager::instance().DeactivateDragonSoul(GetInventoryItem(i), true);
	}
}

#ifdef ENABLE_DS_SET
// Ported 17 September 2026 (Dragon Soul / Alchemy integration) from the
// reference package's char_dragonsoul.cpp, adapted to this server's own
// Dragon Soul deck storage: the reference keeps deck slots in the
// EQUIPMENT window starting right after WEAR_MAX_NUM (its own
// DRAGON_SOUL_EQUIP_SLOT_START = WEAR_MAX_NUM) and reads them with
// GetWear(); this server keeps them in the INVENTORY window's extended
// tail instead (DRAGON_SOUL_EQUIP_SLOT_START = INVENTORY_MAX_NUM +
// WEAR_MAX_NUM, confirmed already wired that way throughout char_item.cpp/
// item.cpp/exchange.cpp/cmd_gm.cpp before this integration started) and
// reads them with GetInventoryItem(), same as every function above already
// does -- not switched to the reference's window scheme. bMaxWear is
// DS_SLOT_MAX itself (not a hardcoded "DS_SLOT6 + 1"), so a future slot
// count change only needs editing in one place (common/item_length.h's
// EDragonSoulSubType), per the operator's explicit instruction.
void CHARACTER::DragonSoul_SetBonus()
{
	RemoveAffect(AFFECT_DS_SET);

	if (!DragonSoul_IsDeckActivated())
		return;

	const int iDeckIdx = DragonSoul_GetActiveDeck();
	if (iDeckIdx < DRAGON_SOUL_DECK_0 || iDeckIdx >= DRAGON_SOUL_DECK_MAX_NUM)
		return;

	const int iStartSlot = DRAGON_SOUL_EQUIP_SLOT_START + DS_SLOT_MAX * iDeckIdx;
	const int iEndSlot = iStartSlot + DS_SLOT_MAX;

	std::vector<BYTE> vDSGrade(DS_SLOT_MAX, 0);
	BYTE bMaxWear = DS_SLOT_MAX;
	BYTE bWearCount = 0;

	for (int i = iStartSlot; i < iEndSlot; ++i)
	{
		LPITEM pItem = GetInventoryItem(i);
		if (NULL == pItem)
			continue;

		if (!DSManager::instance().IsActiveDragonSoul(pItem) ||
			!DSManager::instance().IsTimeLeftDragonSoul(pItem))
			continue;

		vDSGrade[bWearCount] = (pItem->GetVnum() / 1000) % 10;
		bWearCount += 1;
	}

	if (bWearCount >= bMaxWear && vDSGrade[0] > DRAGON_SOUL_GRADE_ANCIENT)
	{
		DragonSoul_CleanUp();

		if (std::equal(vDSGrade.begin() + 1, vDSGrade.begin() + bMaxWear, vDSGrade.begin()))
			AddAffect(AFFECT_DS_SET, POINT_NONE, vDSGrade[0], 0, INFINITE_AFFECT_DURATION, 0, true);

		DragonSoul_ActivateAll();
	}
}

void CHARACTER::DragonSoul_ActivateAll()
{
	for (int i = DRAGON_SOUL_EQUIP_SLOT_START; i < DRAGON_SOUL_EQUIP_SLOT_END; i++)
		DSManager::instance().ActivateDragonSoul(GetInventoryItem(i));
}
#endif

bool CHARACTER::DragonSoul_RefineWindow_Open(LPENTITY pEntity)
{
	if (NULL == m_pointsInstant.m_pDragonSoulRefineWindowOpener)
	{
		m_pointsInstant.m_pDragonSoulRefineWindowOpener = pEntity;
	}

	TPacketGCDragonSoulRefine PDS;
	PDS.header = HEADER_GC_DRAGON_SOUL_REFINE;
	PDS.bSubType = DS_SUB_HEADER_OPEN;
	LPDESC d = GetDesc();

	if (NULL == d)
	{
		sys_err ("User(%s)'s DESC is NULL POINT.", GetName());
		return false;
	}

	d->Packet(&PDS, sizeof(PDS));
	return true;
}

#ifdef ENABLE_DS_CHANGE_ATTR
bool CHARACTER::DragonSoul_RefineWindow_ChangeAttr_Open(LPENTITY pEntity)
{
	if (NULL == m_pointsInstant.m_pDragonSoulRefineWindowOpener)
	{
		m_pointsInstant.m_pDragonSoulRefineWindowOpener = pEntity;
	}

	TPacketGCDragonSoulRefine PDS;
	PDS.header = HEADER_GC_DRAGON_SOUL_REFINE;
	PDS.bSubType = DS_SUB_HEADER_OPEN_CHANGE_ATTR;
	LPDESC d = GetDesc();

	if (NULL == d)
	{
		sys_err ("User(%s)'s DESC is NULL POINT.", GetName());
		return false;
	}

	d->Packet(&PDS, sizeof(PDS));
	return true;
}
#endif

bool CHARACTER::DragonSoul_RefineWindow_Close()
{
	m_pointsInstant.m_pDragonSoulRefineWindowOpener = NULL;
	return true;
}

bool CHARACTER::DragonSoul_RefineWindow_CanRefine()
{
	return NULL != m_pointsInstant.m_pDragonSoulRefineWindowOpener;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
