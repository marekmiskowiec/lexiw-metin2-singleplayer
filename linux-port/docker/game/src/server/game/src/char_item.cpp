#include "stdafx.h"

#include <stack>
#include <cryptopp/osrng.h>

#include "utils.h"
#include "config.h"
#include "char.h"
#include "char_manager.h"
// MT2009_PLUS_MOUNT_ON_EQUIP_V1 (include)
#include "MountSystem.h"
#include "item_manager.h"
#include "sectree_manager.h"
#include "desc.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "packet.h"
#include "affect.h"
#include "skill.h"
#include "start_position.h"
#include "mob_manager.h"
#include "db.h"
#include "log.h"
#include "vector.h"
#include "buffer_manager.h"
#include "questmanager.h"
// playerbot: defined in char_skill.cpp (playerbotify apply_book_wait).
extern int M2SkillBookLearnDelay();
extern time_t M2SkillBookReadAt(const CHARACTER* ch, DWORD dwVnum);
#include "fishing.h"
#include "party.h"
#include "dungeon.h"
#include "refine.h"
#include "unique_item.h"
#include "war_map.h"
#include "xmas_event.h"
#include "marriage.h"
#include "monarch.h"
#include "polymorph.h"
#include "blend_item.h"
#include "castle.h"
#include "arena.h"
#include "threeway_war.h"

#include "safebox.h"
#include "shop.h"
// MT2009_PLUS_SALE_TAX_V1 (char_item include), server-patches/saletax: the panels' sale tax.
#include "playerbot_sale_tax.h"

#ifdef ENABLE_NEWSTUFF
#include "pvp.h"
#include "../../common/PulseManager.h"
#endif

#include "../../common/VnumHelper.h"
#include "DragonSoul.h"
#include "belt_inventory_helper.h"
#include "../../common/CommonDefines.h"
#include "PetSystem.h"
// MT2009_PLUS_AWAKENING_V1 (declare): Digi Rasta's Ritual of Awakening and soul stones +5..+9
// (MT2009_PLUS_SOULSTONE9_V1), defined in playerbot_awakening.h (server-patches/digirasta).
DWORD AwakeningSpecialRefineResult(DWORD dwVnum);
DWORD AwakeningSpecialRefineSet(DWORD dwVnum);
bool AwakeningIsAwakenedWeapon(DWORD dwVnum);

#define ENABLE_EFFECT_EXTRAPOT
#define ENABLE_BOOKS_STACKFIX
#define ENABLE_ITEM_RARE_ATTR_LEVEL_PCT
#define ENABLE_UNIQUE_ITEM_AUTOSPLIT

enum {ITEM_BROKEN_METIN_VNUM = 28960};

// MT2009 Plus: Cor Draconis items from the newer client data must remain
// stackable even when the legacy server proto still carries ANTI_STACK.
static bool IsStackableCorDraconisVnum(DWORD vnum)
{
	switch (vnum)
	{
		case 50252: case 50255: case 50256: case 50257: case 50258: case 50259: case 50260:
		case 51501: case 51502: case 51503: case 51504: case 51505: case 51506: case 51507: case 51508: case 51509: case 51510:
		case 51541: case 51548: case 51549: case 51562: case 51569: case 51576: case 51583: case 51590: case 51597:
		case 51604: case 51611: case 51618: case 51625: case 51632: case 76040:
			return true;
	}
	return false;
}

// CHANGE_ITEM_ATTRIBUTES
const char CHARACTER::msc_szLastChangeItemAttrFlag[] = "Item.LastChangeItemAttr";
// END_OF_CHANGE_ITEM_ATTRIBUTES
const BYTE g_aBuffOnAttrPoints[] = { POINT_ENERGY, POINT_COSTUME_ATTR_BONUS };

struct FFindStone
{
	std::map<DWORD, LPCHARACTER> m_mapStone;

	void operator()(LPENTITY pEnt)
	{
		if (pEnt->IsType(ENTITY_CHARACTER) == true)
		{
			LPCHARACTER pChar = (LPCHARACTER)pEnt;

			if (pChar->IsStone() == true)
			{
				m_mapStone[(DWORD)pChar->GetVID()] = pChar;
			}
		}
	}
};

static bool IS_SUMMON_ITEM(int vnum)
{
	switch (vnum)
	{
		case 22000:
		case 22001:
		case 22010:
		case 22011:
		case 22020:
		case ITEM_MARRIAGE_RING:
			return true;
	}

	return false;
}

static bool IS_MONKEY_DUNGEON(int map_index)
{
	switch (map_index)
	{
		case 5:
		case 25:
		case 45:
		case 108:
		case 109:
			return true;;
	}

	return false;
}

bool IS_SUMMONABLE_ZONE(int map_index)
{
	if (IS_MONKEY_DUNGEON(map_index))
		return false;
	if (IS_CASTLE_MAP(map_index))
		return false;

	switch (map_index)
	{
		case 66 :
		case 71 :
		case 72 :
		case 73 :
		case 193 :
#if 0
		case 184 :
		case 185 :
		case 186 :
		case 187 :
		case 188 :
		case 189 :
#endif

		case 216 :
		case 217 :
		case 208 :

		case 113 :
			return false;
	}

	if (map_index > 10000) return false;

	return true;
}

bool IS_BOTARYABLE_ZONE(int nMapIndex)
{
	if (!g_bEnableBootaryCheck) return true;

	switch (nMapIndex)
	{
		case 1 :
		case 3 :
		case 21 :
		case 23 :
		case 41 :
		case 43 :
			return true;
	}

	return false;
}

static bool FN_check_item_socket(LPITEM item)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		if (item->GetSocket(i) != item->GetProto()->alSockets[i])
			return false;
	}

	return true;
}

static void FN_copy_item_socket(LPITEM dest, LPITEM src)
{
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
	{
		dest->SetSocket(i, src->GetSocket(i));
	}
}
static bool FN_check_item_sex(LPCHARACTER ch, LPITEM item)
{
	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_MALE))
	{
		if (SEX_MALE==GET_SEX(ch))
			return false;
	}

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_FEMALE))
	{
		if (SEX_FEMALE==GET_SEX(ch))
			return false;
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////
// ITEM HANDLING
/////////////////////////////////////////////////////////////////////////////
bool CHARACTER::CanHandleItem(bool bSkipCheckRefine, bool bSkipObserver, DWORD busy_exclude)
{
	if (!bSkipObserver)
		if (m_bIsObserver)
			return false;

	if (IsBusy(busy_exclude))
		return false;

	if (IsBusyAction())
		return false;

	if (!bSkipCheckRefine)
		if (m_bUnderRefine)
		{
			// playerbot: a refine session whose NPC is gone or out of reach
			// has ended (playerbotify apply_refine_abandoned_session).
			const LPCHARACTER refineNPC = m_dwRefineNPCVID ? CHARACTER_MANAGER::instance().Find(m_dwRefineNPCVID) : NULL;
			if (!m_dwRefineNPCVID ||
				(refineNPC && refineNPC->GetMapIndex() == GetMapIndex() &&
				 DISTANCE_APPROX(GetX() - refineNPC->GetX(), GetY() - refineNPC->GetY()) <= 2000))
				return false;

			sys_log(0, "REFINE: %s left a refine session with its NPC gone or out of reach", GetName());
			ClearRefineMode();
		}

	if (NULL != DragonSoul_RefineWindow_GetOpener())
		return false;

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
	if ((m_bAcceCombination) || (m_bAcceAbsorption))
		return false;
#endif

	return true;
}

LPITEM CHARACTER::GetInventoryItem(WORD wCell) const
{
	return GetItem(TItemPos(INVENTORY, wCell));
}
LPITEM CHARACTER::GetItem(TItemPos Cell) const
{
	if (!m_PlayerSlots)
		return nullptr;

	if (!IsValidItemPosition(Cell))
		return NULL;

	WORD wCell = Cell.cell;
	BYTE window_type = Cell.window_type;
	switch (window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
		if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
		{
			sys_err("CHARACTER::GetInventoryItem: invalid item cell %d", wCell);
			return NULL;
		}
		return m_PlayerSlots->pItems[wCell];
	case DRAGON_SOUL_INVENTORY:
		if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
		{
			sys_err("CHARACTER::GetInventoryItem: invalid DS item cell %d", wCell);
			return NULL;
		}
		return m_PlayerSlots->pDSItems[wCell];

	default:
		return NULL;
	}
	return NULL;
}

void CHARACTER::SetItem(TItemPos Cell, LPITEM pItem
	#ifdef ENABLE_HIGHLIGHT_NEW_ITEM
	, bool bWereMine
	#endif
)
{
	if (!m_PlayerSlots)
		return;

	WORD wCell = Cell.cell;
	BYTE window_type = Cell.window_type;
	if ((unsigned long)((CItem*)pItem) == 0xff || (unsigned long)((CItem*)pItem) == 0xffffffff)
	{
		sys_err("!!! FATAL ERROR !!! item == 0xff (char: %s cell: %u)", GetName(), wCell);
		core_dump();
		return;
	}

	if (pItem && pItem->GetOwner())
	{
		assert(!"GetOwner exist");
		return;
	}

	switch(window_type)
	{
	case INVENTORY:
	case EQUIPMENT:
		{
			if (wCell >= INVENTORY_AND_EQUIP_SLOT_MAX)
			{
				sys_err("CHARACTER::SetItem: invalid item cell %d", wCell);
				return;
			}

			LPITEM pOld = m_PlayerSlots->pItems[wCell];

			if (pOld)
			{
				if (wCell < INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pOld->GetSize(); ++i)
					{
						int p = wCell + (i * 5);

						if (p >= INVENTORY_MAX_NUM)
							continue;

						if (m_PlayerSlots->pItems[p] && m_PlayerSlots->pItems[p] != pOld)
							continue;

						m_PlayerSlots->bItemGrid[p] = 0;
					}
				}
				else
					m_PlayerSlots->bItemGrid[wCell] = 0;
			}

			if (pItem)
			{
				if (wCell < INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pItem->GetSize(); ++i)
					{
						int p = wCell + (i * 5);

						if (p >= INVENTORY_MAX_NUM)
							continue;

						m_PlayerSlots->bItemGrid[p] = wCell + 1;
					}
				}
				else
					m_PlayerSlots->bItemGrid[wCell] = wCell + 1;
			}

			m_PlayerSlots->pItems[wCell] = pItem;
		}
		break;

	case DRAGON_SOUL_INVENTORY:
		{
			LPITEM pOld = m_PlayerSlots->pDSItems[wCell];

			if (pOld)
			{
				if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pOld->GetSize(); ++i)
					{
						int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							continue;

						if (m_PlayerSlots->pDSItems[p] && m_PlayerSlots->pDSItems[p] != pOld)
							continue;

						m_PlayerSlots->wDSItemGrid[p] = 0;
					}
				}
				else
					m_PlayerSlots->wDSItemGrid[wCell] = 0;
			}

			if (pItem)
			{
				if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					sys_err("CHARACTER::SetItem: invalid DS item cell %d", wCell);
					return;
				}

				if (wCell < DRAGON_SOUL_INVENTORY_MAX_NUM)
				{
					for (int i = 0; i < pItem->GetSize(); ++i)
					{
						int p = wCell + (i * DRAGON_SOUL_BOX_COLUMN_NUM);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							continue;

						m_PlayerSlots->wDSItemGrid[p] = wCell + 1;
					}
				}
				else
					m_PlayerSlots->wDSItemGrid[wCell] = wCell + 1;
			}

			m_PlayerSlots->pDSItems[wCell] = pItem;
		}
		break;
	default:
		sys_err ("Invalid Inventory type %d", window_type);
		return;
	}

	if (GetDesc())
	{
		if (pItem)
		{
			TPacketGCItemSet pack;
			pack.header = HEADER_GC_ITEM_SET;
			pack.Cell = Cell;

			pack.count = pItem->GetCount();
			pack.vnum = pItem->GetVnum();
			pack.flags = pItem->GetFlag();
			pack.anti_flags	= pItem->GetAntiFlag();
#ifdef ENABLE_HIGHLIGHT_NEW_ITEM
			pack.highlight = !bWereMine || (Cell.window_type == DRAGON_SOUL_INVENTORY);
#else
			pack.highlight = (Cell.window_type == DRAGON_SOUL_INVENTORY);
#endif

			thecore_memcpy(pack.alSockets, pItem->GetSockets(), sizeof(pack.alSockets));
			thecore_memcpy(pack.aAttr, pItem->GetAttributes(), sizeof(pack.aAttr));

			GetDesc()->Packet(&pack, sizeof(TPacketGCItemSet));
		}
		else
		{
			TPacketGCItemDelDeprecated pack;
			pack.header = HEADER_GC_ITEM_DEL;
			pack.Cell = Cell;
			pack.count = 0;
			pack.vnum = 0;
			memset(pack.alSockets, 0, sizeof(pack.alSockets));
			memset(pack.aAttr, 0, sizeof(pack.aAttr));

			GetDesc()->Packet(&pack, sizeof(TPacketGCItemDelDeprecated));
		}
	}

	if (pItem)
	{
		pItem->SetCell(this, wCell);
		switch (window_type)
		{
		case INVENTORY:
		case EQUIPMENT:
			if ((wCell < INVENTORY_MAX_NUM) || (BELT_INVENTORY_SLOT_START <= wCell && BELT_INVENTORY_SLOT_END > wCell))
				pItem->SetWindow(INVENTORY);
			else
				pItem->SetWindow(EQUIPMENT);
			break;
		case DRAGON_SOUL_INVENTORY:
			pItem->SetWindow(DRAGON_SOUL_INVENTORY);
			break;
		}
	}
#ifdef ENABLE_ITEM_SAFE_FLUSH
	if (pItem && m_PlayerSlots)
		m_PlayerSlots->setTouchedItems.emplace(pItem->GetID());
#endif
}

LPITEM CHARACTER::GetWear(BYTE bCell) const
{
	if (!m_PlayerSlots)
		return nullptr;

	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
	{
		sys_err("CHARACTER::GetWear: invalid wear cell %d", bCell);
		return NULL;
	}

	return m_PlayerSlots->pItems[INVENTORY_MAX_NUM + bCell];
}

void CHARACTER::SetWear(BYTE bCell, LPITEM item)
{
	if (bCell >= WEAR_MAX_NUM + DRAGON_SOUL_DECK_MAX_NUM * DS_SLOT_MAX)
	{
		sys_err("CHARACTER::SetItem: invalid item cell %d", bCell);
		return;
	}

	SetItem(TItemPos (INVENTORY, INVENTORY_MAX_NUM + bCell), item);
}

void CHARACTER::ClearItem()
{
	int		i;
	LPITEM	item;

	for (i = 0; i < INVENTORY_AND_EQUIP_SLOT_MAX; ++i)
	{
		if ((item = GetInventoryItem(i)))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);

			SyncQuickslot(QUICKSLOT_TYPE_ITEM, i, 255);
		}
	}
	for (i = 0; i < DRAGON_SOUL_INVENTORY_MAX_NUM; ++i)
	{
		if ((item = GetItem(TItemPos(DRAGON_SOUL_INVENTORY, i))))
		{
			item->SetSkipSave(true);
			ITEM_MANAGER::instance().FlushDelayedSave(item);

			item->RemoveFromCharacter();
			M2_DESTROY_ITEM(item);
		}
	}
}

bool CHARACTER::IsEmptyItemGrid(TItemPos Cell, BYTE bSize, int iExceptionCell) const
{
	if (!m_PlayerSlots)
		return false;

	switch (Cell.window_type)
	{
	case HORSE_INVENTORY:
	case INVENTORY:
		{
			WORD bCell = Cell.cell;

			++iExceptionCell;

			BYTE MAX_INVENTORY = GetInventoryMaxCount();
			BYTE PAGE_COUNT = INVENTORY_DEFAULT_PAGE_COUNT;

			if (CanUseHorseInventory())
			{
				PAGE_COUNT = INVENTORY_PAGE_COUNT;
			}

			if (Cell.IsBeltInventoryPosition())
			{
				LPITEM beltItem = GetWear(WEAR_BELT);

				if (NULL == beltItem)
					return false;

				if (false == CBeltInventoryHelper::IsAvailableCell(bCell - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
					return false;

				if (m_PlayerSlots->bItemGrid[bCell])
				{
					if (m_PlayerSlots->bItemGrid[bCell] == iExceptionCell)
						return true;

					return false;
				}

				if (bSize == 1)
					return true;

			}
			else if (bCell >= MAX_INVENTORY)
				return false;
			// MT2009_CLASSIC_EDITION_V1 (bag pages): MT2009 Classic's bag is the package's
			// two pages - cells 90-179 (pages three and four) take nothing; the saddlebag
			// page after them stays.
			else if (bCell >= INVENTORY_PAGE_SIZE * 2 && bCell < INVENTORY_DEFAULT_MAX_NUM && Mt2009IsClassic())
				return false;

			WORD* gridPtr = m_PlayerSlots->bItemGrid.data();
			if (m_PlayerSlots->bItemGrid[bCell])
			{
				if (m_PlayerSlots->bItemGrid[bCell] == iExceptionCell)
				{
					return is_empty_page_grid(gridPtr, bCell, bSize, iExceptionCell, INVENTORY_PAGE_COLUMN, INVENTORY_PAGE_ROW, PAGE_COUNT, true, MAX_INVENTORY);
				}
				else
					return false;
			}

			return is_empty_page_grid(gridPtr, bCell, bSize, iExceptionCell, INVENTORY_PAGE_COLUMN, INVENTORY_PAGE_ROW, PAGE_COUNT, true, MAX_INVENTORY);
		}
		break;
	case DRAGON_SOUL_INVENTORY:
		{
			WORD wCell = Cell.cell;
			if (wCell >= DRAGON_SOUL_INVENTORY_MAX_NUM)
				return false;

			iExceptionCell++;

			if (m_PlayerSlots->wDSItemGrid[wCell])
			{
				if (m_PlayerSlots->wDSItemGrid[wCell] == iExceptionCell)
				{
					if (bSize == 1)
						return true;

					int j = 1;

					do
					{
						int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

						if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
							return false;

						if (m_PlayerSlots->wDSItemGrid[p])
							if (m_PlayerSlots->wDSItemGrid[p] != iExceptionCell)
								return false;
					}
					while (++j < bSize);

					return true;
				}
				else
					return false;
			}

			if (1 == bSize)
				return true;
			else
			{
				int j = 1;

				do
				{
					int p = wCell + (DRAGON_SOUL_BOX_COLUMN_NUM * j);

					if (p >= DRAGON_SOUL_INVENTORY_MAX_NUM)
						return false;

					if (m_PlayerSlots->wDSItemGrid[p]) // @fixme159 (from bItemGrid)
						if (m_PlayerSlots->wDSItemGrid[p] != iExceptionCell)
							return false;
				}
				while (++j < bSize);

				return true;
			}
		}
	}
	return false;
}

bool CHARACTER::HasSlotForItem(DWORD dwItemVnum) const
{
	TItemTable* pTable = ITEM_MANAGER::instance().GetTable(dwItemVnum);
	if (!pTable)
	{
		return false;
	}
	return GetEmptyInventory(pTable->bSize) != -1;
}

int CHARACTER::GetEmptyInventoryEx(LPITEM item)
{
	if (!item)
		return -1;

	int cell = -1;
	if (item->IsDragonSoul())
		cell = GetEmptyDragonSoulInventory(item);
	// else if (item->IsSpecialInventory()) // example
	// 	cell = GetEmptySpecialInventory(item);
	else
		cell = GetEmptyInventory(item->GetSize());

	return cell;
}

int CHARACTER::GetEmptyInventory(BYTE size, BYTE startSlot) const
{
	if (startSlot >= GetInventoryMaxCount())
		return -1;

	for ( int i = startSlot; i < GetInventoryMaxCount(); ++i)
		if (IsEmptyItemGrid(TItemPos (INVENTORY, i), size))
			return i;
	return -1;
}

int CHARACTER::GetEmptyDragonSoulInventory(LPITEM pItem) const
{
	if (NULL == pItem || !pItem->IsDragonSoul())
		return -1;
	if (!DragonSoul_IsQualified())
	{
		return -1;
	}
	BYTE bSize = pItem->GetSize();
	WORD wBaseCell = DSManager::instance().GetBasePosition(pItem);

	if (WORD_MAX == wBaseCell)
		return -1;

	for (int i = 0; i < DRAGON_SOUL_BOX_SIZE; ++i)
		if (IsEmptyItemGrid(TItemPos(DRAGON_SOUL_INVENTORY, i + wBaseCell), bSize))
			return i + wBaseCell;

	return -1;
}

void CHARACTER::CopyDragonSoulItemGrid(std::vector<WORD>& vDragonSoulItemGrid) const
{
	vDragonSoulItemGrid.resize(DRAGON_SOUL_INVENTORY_MAX_NUM);

	std::copy(m_PlayerSlots->wDSItemGrid.data(), m_PlayerSlots->wDSItemGrid.data() + DRAGON_SOUL_INVENTORY_MAX_NUM, vDragonSoulItemGrid.begin());
}

int CHARACTER::CountEmptyInventory() const
{
	int	count = 0;

	for (int i = 0; i < GetInventoryMaxCount(); ++i)
		if (GetInventoryItem(i))
			count += GetInventoryItem(i)->GetSize();

	return (GetInventoryMaxCount() - count);
}

void TransformRefineItem(LPITEM pkOldItem, LPITEM pkNewItem)
{
	// ACCESSORY_REFINE
	if (pkOldItem->IsAccessoryForSocket())
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			pkNewItem->SetSocket(i, pkOldItem->GetSocket(i));
		}
		//pkNewItem->StartAccessorySocketExpireEvent();
	}
	// END_OF_ACCESSORY_REFINE
	else
	{
		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			if (!pkOldItem->GetSocket(i))
				break;
			else
				pkNewItem->SetSocket(i, 1);
		}

		int slot = 0;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		{
			long socket = pkOldItem->GetSocket(i);

			if (socket > 2 && socket != ITEM_BROKEN_METIN_VNUM)
				pkNewItem->SetSocket(slot++, socket);
		}

	}

	pkOldItem->CopyAttributeTo(pkNewItem);
}

void NotifyRefineSuccess(LPCHARACTER ch, LPITEM item, const char* way)
{
	if (NULL != ch && item != NULL)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineSuceeded");

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), 1, way);
	}
}

void NotifyRefineFail(LPCHARACTER ch, LPITEM item, const char* way, int success = 0)
{
	if (NULL != ch && NULL != item)
	{
		// MT2009_PLUS_DIGI_SERVER_QOL_V1 (refine note): what the item was, for RefineFailedType (playerbot_digi_qol.h, Autor: Digi Rasta).
		{ void Mt2009DigiRefineFailNoted(LPCHARACTER, LPITEM); Mt2009DigiRefineFailNoted(ch, item); }
		ch->ChatPacket(CHAT_TYPE_COMMAND, "RefineFailed");

		LogManager::instance().RefineLog(ch->GetPlayerID(), item->GetName(), item->GetID(), item->GetRefineLevel(), success, way);
	}
}

void CHARACTER::SetRefineNPC(LPCHARACTER ch)
{
	if ( ch != NULL )
	{
		m_dwRefineNPCVID = ch->GetVID();
	}
	else
	{
		m_dwRefineNPCVID = 0;
	}
}

bool CHARACTER::IsRefineBonus(LPITEM scrollItem, DWORD bonusFlag)
{
	if (!scrollItem ||
		!(scrollItem->GetType() == ITEM_USE && scrollItem->GetSubType() == USE_TUNING && scrollItem->GetValue(0) == REFINE_BONUS_SCROLL))
		return false;

	return IS_SET(scrollItem->GetValue(2), bonusFlag);
}

bool CHARACTER::CheckRefineBonus(LPITEM item, LPITEM scrollItem)
{
	if (!item)
		return false;

	if (!scrollItem)
		return false;

		const int RefineItemType = scrollItem->GetValue(3);
		const int RefineMaxUseLevel = scrollItem->GetValue(4);
		const int RefineMaxRefineLevel = scrollItem->GetValue(5);

	if (!item->CheckItemUseLevel(RefineMaxUseLevel))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("REFINE_BONUS_USE_LEVEL_ERROR"), RefineMaxUseLevel);
		return false;
	}

	if (item->GetRefineLevel() > RefineMaxRefineLevel - 1)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("REFINE_BONUS_REFINE_LEVEL_ERROR"), RefineMaxRefineLevel - 1);
		return false;
	}

	if (RefineItemType != REFINE_BONUS_ITEM_ALL)
	{
		bool isArmorSubType = item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_HEAD || item->GetSubType() == ARMOR_SHIELD;

		if (RefineItemType == REFINE_BONUS_ITEM_ARMOR && !(item->GetType() == ITEM_ARMOR && isArmorSubType))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("REFINE_BONUS_ONLY_ARMOR"));
			return false;
		}

		if (RefineItemType == REFINE_BONUS_ITEM_JEWELRY && !(item->GetType() == ITEM_ARMOR && !isArmorSubType))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("REFINE_BONUS_ONLY_JEWELRY"));
			return false;
		}

		if (RefineItemType == REFINE_BONUS_ITEM_WEAPON && item->GetType() != ITEM_WEAPON)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("REFINE_BONUS_ONLY_WEAPON"));
			return false;
		}
	}
	return true;
}

// MT2009_PLUS_DIGI_STACK_V1 (refine stack helpers): Autor: Digi Rasta (nowy-system v0.23,
// stacking; server-patches/digirasta-fixes). Soul stones stack now (apply.sh), so the
// Blacksmith may be handed a stack: the stack keeps ONE piece in its cell for the refine
// and the rest moves to a free cell first - the success, the failure and the lowered
// result below only ever touch that one piece, never the whole stack and never a copy.
// The rest moves only after every check and payment passed (DoRefine,
// DoRefineWithScroll), and only when a free cell is there (checked before anything is
// taken).
static bool DigiStackRefineAllowed(LPCHARACTER ch, LPITEM item, const TRefineTable* prt)
{
	if (!ch || !item || item->GetCount() <= 1)
		return true;
	// A recipe taking the item's own vnum could take the very piece being refined.
	for (int i = 0; prt && i < prt->material_count; ++i)
		if (prt->materials[i].vnum == item->GetVnum())
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be improved."));
			return false;
		}
	if (item->IsExchanging() || item->isLocked() || ch->GetEmptyInventory(item->GetSize()) < 0)
	{
		ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There isn't enough space in your inventory."));
		return false;
	}
	return true;
}

static bool DigiStackRefineSplit(LPCHARACTER ch, LPITEM item)
{
	if (!ch || !item || item->GetCount() <= 1)
		return true;
	const int iCell = ch->GetEmptyInventory(item->GetSize());
	if (iCell < 0)
		return false;
	LPITEM pkRest = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), item->GetCount() - 1, 0, false);
	if (!pkRest)
		return false;
	pkRest->SetSockets(item->GetSockets());
	pkRest->SetAttributes(item->GetAttributes());
	if (!pkRest->AddToCharacter(ch, TItemPos(INVENTORY, iCell)))
	{
		M2_DESTROY_ITEM(pkRest);
		return false;
	}
	item->SetCount(1);
	ITEM_MANAGER::instance().FlushDelayedSave(pkRest);
	LogManager::instance().ItemLog(ch, pkRest, "REFINE STACK REST", pkRest->GetName());
	return true;
}

bool CHARACTER::DoRefine(LPITEM item, bool bMoneyOnly, BYTE refineType)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	// MT2009_PLUS_AWAKENING_V1 (do refine): the refines item_proto does not carry - a weapon
	// 75 +9 into its awakened weapon +0 (the Ritual of Awakening, recipe 7110) and a
	// soul stone +4..+8 one grade up (MT2009_PLUS_SOULSTONE9_V1, recipes 7204-7208) - at a
	// plain smith only; neither they nor an awakened weapon at a guild smith or in
	// the Demon Tower (its fee multiplier would overflow at these prices).
	const DWORD dwAwakeningResult = (!bMoneyOnly && refineType == REFINE_TYPE_NORMAL) ? AwakeningSpecialRefineResult(item->GetVnum()) : 0;
	if ((dwAwakeningResult || AwakeningIsAwakenedWeapon(item->GetVnum())) && (bMoneyOnly || IsRefineThroughGuild()))
		return false;
	const TRefineTable * prt = CRefineManager::instance().GetRefineRecipe(dwAwakeningResult ? AwakeningSpecialRefineSet(item->GetVnum()) : item->GetRefineSet());

	if (!prt)
		return false;

	DWORD result_vnum = dwAwakeningResult ? dwAwakeningResult : item->GetRefinedVnum();
	int cost = ComputeRefineFee(prt->cost);

	if (result_vnum == 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No further improvements possible."));
		return false;
	}

	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
		return false;

	// MT2009_PLUS_AWAKENING_V1 (do refine proto): the result's proto, not the refined vnum's
	// (0 for the ritual and the soul stones).
	TItemTable * pProto = ITEM_MANAGER::instance().GetTable(result_vnum);

	if (!pProto)
	{
		sys_err("DoRefine NOT GET ITEM PROTO %u", result_vnum);
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be improved."));
		return false;
	}

	// MT2009_PLUS_DIGI_STACK_V1 (do refine room): a stack needs a free cell for its rest.
	if (!DigiStackRefineAllowed(this, item, prt))
		return false;

	// REFINE_COST
	if (GetGold() < cost)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You do not have enough Yang to use this item."));
		return false;
	}

	// MT2009_PLUS_GOBLIN_V1 (refine free): the Treasure Hunt's blessing - the
	// next refine takes no materials, and the blessing is spent.
	bool GoblinRefineFree(LPCHARACTER ch, bool consume);
	if (!(bMoneyOnly) && !GoblinRefineFree(this, true))
	{
		for (int i = 0; i < prt->material_count; ++i)
		{
			if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
			{
				if (test_server)
				{
					ChatPacket(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, CountSpecifyItem(prt->materials[i].vnum), prt->materials[i].count);
				}
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Not the right material for an upgrade."));
				return false;
			}
		}

		for (int i = 0; i < prt->material_count; ++i)
			RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);
	}
	// END_OF_REFINE_COST

	// MT2009_PLUS_DIGI_STACK_V1 (do refine split): paid - one piece of a stack stays for
	// the roll, the rest moves out of the way (never refined, never destroyed).
	if (!DigiStackRefineSplit(this, item))
	{
		sys_err("DIGI_STACK: refine split failed pid=%u vnum=%u count=%u", GetPlayerID(), item->GetVnum(), (unsigned)item->GetCount());
		return false;
	}

	int prob = number(1, 100);

	if (IsRefineThroughGuild())
		prob -= 10;

	// MT2009_PLUS_GOBLIN_V1 (refine pct): the Treasure Hunt's blessing, +10 %
	// on this refine (spent).
	{
		int GoblinRefineBonus(LPCHARACTER ch, bool consume);
		prob -= GoblinRefineBonus(this, true);
	}

	if (test_server)
	{
		ChatDebug("REFINE random %d proto chance %d%%", prob, prt->prob);
	}

	bool bIsDestroyOnFail = true;
	DWORD result_fail_vnum = 0;

	if (refineType == REFINE_TYPE_FISHER ||
		refineType == REFINE_TYPE_HERB_KNIFE ||
		refineType == REFINE_TYPE_PICKAXE)
	{
		bIsDestroyOnFail = false;
		result_fail_vnum = item->GetValue(4);

		if (refineType == REFINE_TYPE_FISHER)
		{
			result_fail_vnum = item->GetVnum();
		}
	}

	// MT2009_PLUS_AWAKENING_V1 (do refine no burn): an awakened weapon is neither destroyed
	// nor lowered by a failed refine - it stays as it was (Digi Rasta).
	if (AwakeningIsAwakenedWeapon(item->GetVnum()))
	{
		bIsDestroyOnFail = false;
		result_fail_vnum = item->GetVnum();
	}

	// MT2009_PLUS_BATTLE_PASS_V1 (refine try): every refine attempt at a
	// smith, won or lost, for the Battle Pass.
	{
		void BattlePassOnRefine(LPCHARACTER ch);
		BattlePassOnRefine(this);
	}
	if (prob <= prt->prob)
	{
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			AddPlayerStat(PLAYER_STATS_REFINE_SUCCESS_FLAG);

			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			BYTE bCell = item->GetCell();

			// MT2009_PLUS_AWAKENING_V1 (sockets): an awakened weapon has its three sockets,
			// whatever the weapon it came from had.
			if (AwakeningIsAwakenedWeapon(pkNewItem->GetVnum()))
				for (int iAwakeningSocket = 0; iAwakeningSocket < 3; ++iAwakeningSocket)
					if (pkNewItem->GetSocket(iAwakeningSocket) == 0)
						pkNewItem->SetSocket(iAwakeningSocket, 1);

			// DETAIL_REFINE_LOG
			NotifyRefineSuccess(this, item, IsRefineThroughGuild() ? "GUILD" : (bMoneyOnly ? "DEVILTOWER" : "POWER"));
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");
			// END_OF_DETAIL_REFINE_LOG

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

			sys_log(0, "Refine Success %d", cost);
			pkNewItem->AttrLog();
			sys_log(0, "PayPee %d", cost);
			PayRefineFee(cost);
			sys_log(0, "PayPee End %d", cost);

			if (pkNewItem->GetType() == ITEM_ROD ||
				pkNewItem->GetType() == ITEM_HERB_KNIFE ||
				pkNewItem->GetType() == ITEM_PICK)
			{
				pkNewItem->SetSocket(0, 0);
			}
		}
		else
		{
			// DETAIL_REFINE_LOG

			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : (bMoneyOnly ? "DEVILTOWER" : "POWER"));
			// END_OF_DETAIL_REFINE_LOG
		}
	}
	else
	{
		if (refineType == REFINE_TYPE_NORMAL || refineType == REFINE_TYPE_MONEY_ONLY)
			AddPlayerStat(PLAYER_STATS_REFINE_FAIL_SMITH_FLAG);

		DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
		NotifyRefineFail(this, item, IsRefineThroughGuild() ? "GUILD" : (bMoneyOnly ? "DEVILTOWER" : "POWER"));

		if (bIsDestroyOnFail)
		{
			item->AttrLog();
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");
		}
		else {
			LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);

			if (pkNewItem)
			{
				ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
				LogManager::instance().ItemLog(this, pkNewItem, "REFINE FAIL", pkNewItem->GetName());

				BYTE bCell = item->GetCell();

				DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -prt->cost);
				ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

				pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
				ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

				pkNewItem->AttrLog();

				if (pkNewItem->GetType() == ITEM_ROD ||
					pkNewItem->GetType() == ITEM_HERB_KNIFE ||
					pkNewItem->GetType() == ITEM_PICK)
				{
					pkNewItem->SetSocket(0, 0);
				}
			}
			else
			{
				sys_err("cannot create item %u", result_fail_vnum);
			}
		}

		PayRefineFee(cost);
	}

	return true;
}

bool CHARACTER::DoRefineWithScroll(LPITEM item)
{
	if (!CanHandleItem(true))
	{
		ClearRefineMode();
		return false;
	}

	ClearRefineMode();

	if (quest::CQuestManager::instance().GetEventFlag("update_refine_time") != 0)
	{
		if (get_global_time() < quest::CQuestManager::instance().GetEventFlag("update_refine_time") + (60 * 5))
		{
			sys_log(0, "can't refine %d %s", GetPlayerID(), GetName());
			return false;
		}
	}

	const TRefineTable * prt = CRefineManager::instance().GetRefineRecipe(item->GetRefineSet());

	if (!prt)
		return false;

	LPITEM pkItemScroll;

	if (m_iRefineAdditionalCell < 0)
		return false;

	pkItemScroll = GetInventoryItem(m_iRefineAdditionalCell);

	if (!pkItemScroll)
		return false;

	if (!(pkItemScroll->GetType() == ITEM_USE && pkItemScroll->GetSubType() == USE_TUNING))
		return false;

	if (pkItemScroll->GetVnum() == item->GetVnum())
		return false;

	DWORD result_vnum = item->GetRefinedVnum();
	DWORD result_fail_vnum = item->GetRefineFromVnum();

	if (result_vnum == 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No further improvements possible."));
		return false;
	}

	// MUSIN_SCROLL
	if (pkItemScroll->GetValue(0) == UP_TO_3TH_LEVEL_SCROLL)
	{
		if (item->GetRefineLevel() >= 4)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot upgrade items with this Scroll."));
			return false;
		}
	}
	// END_OF_MUSIC_SCROLL

	TItemTable * pProto = ITEM_MANAGER::instance().GetTable(item->GetRefinedVnum());

	if (!pProto)
	{
		sys_err("DoRefineWithScroll NOT GET ITEM PROTO %d", item->GetRefinedVnum());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be improved."));
		return false;
	}

	// MT2009_PLUS_DIGI_STACK_V1 (scroll room): a stack needs a free cell for its rest.
	if (!DigiStackRefineAllowed(this, item, prt))
		return false;

	if (GetGold() < prt->cost)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You do not have enough Yang to use this item."));
		return false;
	}

	int prob = number(1, 100);
	int cost = prt->cost;
	int success_prob = prt->prob;
	bool bDestroyWhenFail = false;
	bool bLevelReductionOnFail = true;
	bool bCheckMaterials = true;

	const char* szRefineType = "SCROLL";

	if (pkItemScroll->GetValue(0) == NO_REDUCTION_WHEN_FAIL_SCROLL)
	{
		bDestroyWhenFail = false;
		bLevelReductionOnFail = false;
		szRefineType = "NO_REDUCTION_WHEN_FAIL_SCROLL";
	}

	if (pkItemScroll->GetValue(0) == UP_TO_3TH_LEVEL_SCROLL)
	{
		success_prob = 100;
		szRefineType = "UP_TO_3TH_LEVEL_SCROLL";
	}

	if (pkItemScroll->GetValue(0) == REFINE_BONUS_SCROLL)
	{
		szRefineType = "REFINE_BONUS_SCROLL";
		bLevelReductionOnFail = false;
		bDestroyWhenFail = true;

		const DWORD bonusFlag = pkItemScroll->GetValue(2);
		if (IS_SET(bonusFlag, TUNING_FLAG_100_CHANCE))
		{
			success_prob = 100;
		}

		if (IS_SET(bonusFlag, TUNING_FLAG_NO_GOLD))
		{
			cost = 0;
		}

		if (IS_SET(bonusFlag, TUNING_FLAG_NO_ITEM))
		{
			bCheckMaterials = false;
		}
	}

	// The scroll by its vnum for the refine log, taken while it exists:
	// SetCount below destroys the last one.
	char szRefineWay[48];
	snprintf(szRefineWay, sizeof(szRefineWay), "SCROLL:%u", pkItemScroll->GetVnum());
	szRefineType = szRefineWay;

	// MT2009_PLUS_AWAKENING_V1 (scroll no burn): nor with a scroll.
	if (AwakeningIsAwakenedWeapon(item->GetVnum()))
	{
		bDestroyWhenFail = false;
		bLevelReductionOnFail = false;
	}

	success_prob += pkItemScroll->GetValue(1);

	// MT2009_PLUS_GOBLIN_V1 (refine scroll): the Treasure Hunt's blessings on a
	// scroll's refine - +10 %, no materials (each spent).
	{
		int GoblinRefineBonus(LPCHARACTER ch, bool consume);
		bool GoblinRefineFree(LPCHARACTER ch, bool consume);
		success_prob += GoblinRefineBonus(this, true);
		if (bCheckMaterials && GoblinRefineFree(this, true))
			bCheckMaterials = false;
	}

	if (bCheckMaterials)
	{
		// check refine items
		for (int i = 0; i < prt->material_count; ++i)
		{
			if (CountSpecifyItem(prt->materials[i].vnum) < prt->materials[i].count)
			{
				if (test_server)
				{
					ChatPacket(CHAT_TYPE_INFO, "Find %d, count %d, require %d", prt->materials[i].vnum, CountSpecifyItem(prt->materials[i].vnum), prt->materials[i].count);
				}
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Not the right material for an upgrade."));
				return false;
			}
		}

		// remove refine items
		for (int i = 0; i < prt->material_count; ++i)
			RemoveSpecifyItem(prt->materials[i].vnum, prt->materials[i].count);
	}

	// MT2009_PLUS_DIGI_STACK_V1 (scroll split): one piece of a stack takes the scroll.
	if (!DigiStackRefineSplit(this, item))
	{
		sys_err("DIGI_STACK: scroll refine split failed pid=%u vnum=%u count=%u", GetPlayerID(), item->GetVnum(), (unsigned)item->GetCount());
		return false;
	}

	pkItemScroll->SetCount(pkItemScroll->GetCount() - 1);

	if (test_server)
	{
		ChatDebug("REFINE type %s chance %d%% (default %d%%)", szRefineType, success_prob, prt->prob);
	}

	// MT2009_PLUS_BATTLE_PASS_V1 (refine try scroll): every refine attempt
	// with a scroll, won or lost, for the Battle Pass.
	{
		void BattlePassOnRefine(LPCHARACTER ch);
		BattlePassOnRefine(this);
	}
	if (prob <= success_prob) // success
	{
		LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_vnum, 1, 0, false);

		if (pkNewItem)
		{
			AddPlayerStat(PLAYER_STATS_REFINE_SUCCESS_FLAG);

			ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
			LogManager::instance().ItemLog(this, pkNewItem, "REFINE SUCCESS", pkNewItem->GetName());

			BYTE bCell = item->GetCell();

			NotifyRefineSuccess(this, item, szRefineType);
			DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE SUCCESS)");

			pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
			ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);
			pkNewItem->AttrLog();
			PayRefineFee(cost);
		}
		else
		{
			sys_err("cannot create item %u", result_vnum);
			NotifyRefineFail(this, item, szRefineType);
		}
	}
	else 
	{
		NotifyRefineFail(this, item, szRefineType);
		PayRefineFee(cost);

		if (bDestroyWhenFail)
		{
			ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");
		}
		else if (bLevelReductionOnFail && result_fail_vnum)
		{
			LPITEM pkNewItem = ITEM_MANAGER::instance().CreateItem(result_fail_vnum, 1, 0, false);
			if (pkNewItem)
			{
				ITEM_MANAGER::CopyAllAttrTo(item, pkNewItem);
				LogManager::instance().ItemLog(this, pkNewItem, "REFINE FAIL", pkNewItem->GetName());

				BYTE bCell = item->GetCell();

				DBManager::instance().SendMoneyLog(MONEY_LOG_REFINE, item->GetVnum(), -cost);
				NotifyRefineFail(this, item, szRefineType, -1);
				ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (REFINE FAIL)");

				pkNewItem->AddToCharacter(this, TItemPos(INVENTORY, bCell));
				ITEM_MANAGER::instance().FlushDelayedSave(pkNewItem);

				pkNewItem->AttrLog();
			}
			else
			{
				sys_err("cannot create item %u", result_fail_vnum);
				NotifyRefineFail(this, item, szRefineType);
			}
		}
	}

	return true;
}

bool CHARACTER::RefineInformation(BYTE bCell, BYTE bType, int iAdditionalCell)
{
	if (bCell > INVENTORY_MAX_NUM)
		return false;

	LPITEM item = GetInventoryItem(bCell);

	if (!item)
		return false;

	// REFINE_COST
	if (bType == REFINE_TYPE_MONEY_ONLY && !GetQuestFlag("deviltower_zone.can_refine"))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can only be rewarded once for the Demon Tower Quest."));
		return false;
	}
	// END_OF_REFINE_COST

	TPacketGCRefineInformation p;

	p.header = HEADER_GC_REFINE_INFORMATION;
	p.pos = bCell;
	p.src_vnum = item->GetVnum();
	p.result_vnum = item->GetRefinedVnum();
	p.type = bType;

	// MT2009_PLUS_AWAKENING_V1 (refine info): the ritual and a soul stone's step show as a refine
	// to their result - at a plain smith's normal window only.
	const DWORD dwAwakeningResult = AwakeningSpecialRefineResult(item->GetVnum());
	if (dwAwakeningResult || AwakeningIsAwakenedWeapon(item->GetVnum()))
	{
		if (IsRefineThroughGuild() || bType == REFINE_TYPE_MONEY_ONLY || (dwAwakeningResult && bType != REFINE_TYPE_NORMAL))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be advanced this way."));
			return false;
		}
		if (dwAwakeningResult)
			p.result_vnum = dwAwakeningResult;
	}

	if (p.result_vnum == 0)
	{
		sys_err("RefineInformation p.result_vnum == 0");
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be improved."));
		return false;
	}

	LPITEM itemScroll = GetInventoryItem(iAdditionalCell);
	if (item->GetType() == ITEM_USE && item->GetSubType() == USE_TUNING)
	{
		if (bType == 0)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be advanced this way."));
			return false;
		}
		else
		{
			if (!itemScroll || item->GetVnum() == itemScroll->GetVnum())
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can combine the Blessing Scroll with the Magic Iron Ore."));
				return false;
			}
		}
	}

	CRefineManager & rm = CRefineManager::instance();

	const TRefineTable* prt = rm.GetRefineRecipe(dwAwakeningResult ? AwakeningSpecialRefineSet(item->GetVnum()) : item->GetRefineSet()); // MT2009_PLUS_AWAKENING_V1 (refine info recipe)

	if (!prt)
	{
		sys_err("RefineInformation NOT GET REFINE SET %d", item->GetRefineSet());
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be improved."));
		return false;
	}

	// REFINE_COST

	if (itemScroll && itemScroll->GetValue(0) == REFINE_BONUS_SCROLL && !CheckRefineBonus(item, itemScroll))
	{
		return false;
	}


	p.cost = IsRefineBonus(itemScroll, TUNING_FLAG_NO_GOLD) ? 0 : ComputeRefineFee(prt->cost);
	// The chance the roll will use, as DoRefine and DoRefineWithScroll count it
	// (apply_refine_chance_shown): the package sent 0 here.
	int iRefineChance = prt->prob;
	if (itemScroll && itemScroll->GetType() == ITEM_USE && itemScroll->GetSubType() == USE_TUNING)
	{
		if (itemScroll->GetValue(0) == UP_TO_3TH_LEVEL_SCROLL || IsRefineBonus(itemScroll, TUNING_FLAG_100_CHANCE))
			iRefineChance = 100;
		iRefineChance += itemScroll->GetValue(1);
	}
	else if (IsRefineThroughGuild())
		iRefineChance += 10;
	// MT2009_PLUS_GOBLIN_V1 (refine info): the chance and the materials shown
	// with the Treasure Hunt's blessings (not spent here).
	int GoblinRefineBonus(LPCHARACTER ch, bool consume);
	bool GoblinRefineFree(LPCHARACTER ch, bool consume);
	iRefineChance += GoblinRefineBonus(this, false);
	p.prob = MINMAX(0, iRefineChance, 100);

	if (bType == REFINE_TYPE_MONEY_ONLY || IsRefineBonus(itemScroll, TUNING_FLAG_NO_ITEM) ||
			(bType != REFINE_TYPE_MONEY_ONLY && GoblinRefineFree(this, false)))
	{
		p.material_count = 0;
		memset(p.materials, 0, sizeof(p.materials));
	}
	else
	{
		p.material_count = prt->material_count;
		thecore_memcpy(&p.materials, prt->materials, sizeof(prt->materials));
	}
	// END_OF_REFINE_COST

	GetDesc()->Packet(&p, sizeof(TPacketGCRefineInformation));

	SetRefineMode(iAdditionalCell);
	return true;
}

bool CHARACTER::RefineItem(LPITEM pkItem, LPITEM pkTarget)
{
	if (!CanHandleItem())
		return false;

	if (pkItem->GetSubType() == USE_TUNING)
	{
		if (pkItem->GetValue(0) == UP_TO_3TH_LEVEL_SCROLL)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_UP_TO_3TH_LEVEL, pkItem->GetCell());
		else if (pkItem->GetValue(0) == NO_REDUCTION_WHEN_FAIL_SCROLL)
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_NO_REDUCTION_WHEN_FAIL, pkItem->GetCell());
		else if (pkItem->GetValue(0) == REFINE_BONUS_SCROLL)
		{
			if (pkTarget && pkTarget->GetType() == ITEM_WEAPON && pkTarget->GetLimitValue(0) == 30) // blokada uzycia na FMSy itp.
				return false;

			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_SCROLL, pkItem->GetCell());
		}
		else
		{
			if (IsEliteRefine(pkTarget->GetRefineSet())) return false;
			RefineInformation(pkTarget->GetCell(), REFINE_TYPE_SCROLL, pkItem->GetCell());
		}
	}
	else if (pkItem->GetSubType() == USE_DETACHMENT && IS_SET(pkTarget->GetFlag(), ITEM_FLAG_REFINEABLE))
	{
		if (!(pkTarget->GetType() == ITEM_WEAPON && pkTarget->GetSubType() <= WEAPON_FAN
			|| pkTarget->GetType() == ITEM_ARMOR && pkTarget->GetSubType() == ARMOR_BODY))
		{
			ChatDebug("cannot detach");
			return false;
		}

		LogManager::instance().ItemLog(this, pkTarget, "USE_DETACHMENT", pkTarget->GetName());

		bool bHasMetinStone = false;

		for (int i = 0; i < ITEM_SOCKET_MAX_NUM; i++)
		{
			long socket = pkTarget->GetSocket(i);
			if (socket >= 28030 && socket <= 28543)
			{
				bHasMetinStone = true;
				break;
			}
		}

		if (bHasMetinStone)
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
			{
				long socket = pkTarget->GetSocket(i);
				if (socket >= 28030 && socket <= 28543)
				{
					const TItemTable* pTable = ITEM_MANAGER::instance().GetTable(socket);
					if (pTable && pTable->bType == ITEM_METIN)
					{
						AutoGiveItem(socket);

						pkTarget->SetSocket(i, ITEM_BROKEN_METIN_VNUM, true, "SOCKET_DETACHMENT");
					}
				}
			}
			pkItem->SetCount(pkItem->GetCount() - 1);
			return true;
		}
		else
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is no Stone available to take out."));
			return false;
		}
	}

	return false;
}

EVENTFUNC(kill_campfire_event)
{
	char_event_info* info = dynamic_cast<char_event_info*>( event->info );

	if ( info == NULL )
	{
		sys_err( "kill_campfire_event> <Factor> Null pointer" );
		return 0;
	}

	LPCHARACTER	ch = info->ch;

	if (ch == NULL) { // <Factor>
		return 0;
	}
	ch->m_pkMiningEvent = NULL;
	M2_DESTROY_CHARACTER(ch);
	return 0;
}

bool CHARACTER::GiveRecallItem(LPITEM item)
{
	int idx = GetMapIndex();
	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
		case 66:
		case 216:
			iEmpireByMapIndex = -1;
			break;
	}

	if (iEmpireByMapIndex < 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot store this location."));
	}
	else if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot save location placed in opposite kingdom."));
		return false;
	}

	int pos;

	if (item->GetCount() == 1)
	{
		item->SetSocket(0, GetX());
		item->SetSocket(1, GetY());
	}
	else if ((pos = GetEmptyInventory(item->GetSize())) != -1)
	{
		LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), 1);

		if (NULL != item2)
		{
			item->SetCount(item->GetCount() - 1);
			item2->SetSocket(0, GetX());
			item2->SetSocket(1, GetY());
			AutoGiveItem(item2); // @fixme316
		}
	}
	else
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There isn't enough space in your inventory."));
		return false;
	}

	return true;
}

void CHARACTER::ProcessRecallItem(LPITEM item)
{
	int idx;

	if ((idx = SECTREE_MANAGER::instance().GetMapIndex(item->GetSocket(0), item->GetSocket(1))) == 0)
		return;

	int iEmpireByMapIndex = -1;

	if (idx < 20)
		iEmpireByMapIndex = 1;
	else if (idx < 40)
		iEmpireByMapIndex = 2;
	else if (idx < 60)
		iEmpireByMapIndex = 3;
	else if (idx < 10000)
		iEmpireByMapIndex = 0;

	switch (idx)
	{
		case 66:
		case 216:
			iEmpireByMapIndex = -1;
			break;

		case 301:
		case 302:
		case 303:
		case 304:
			if( GetLevel() < 90 )
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your level is too low to use this item."));
				return;
			}
			else
				break;
	}

	if (iEmpireByMapIndex && GetEmpire() != iEmpireByMapIndex)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot teleport to a safe position in a foreign Kingdom."));
		item->SetSocket(0, 0);
		item->SetSocket(1, 0);
	}
	else
	{
		sys_log(1, "Recall: %s %d %d -> %d %d", GetName(), GetX(), GetY(), item->GetSocket(0), item->GetSocket(1));
		if (HasPlayerData())
			playerData->SetSummonItemDelay(this, 20);

		SetQuestFlag("summon_item.scroll_delay", get_global_time() + 15 * 60);

		WarpSet(item->GetSocket(0), item->GetSocket(1));
		item->SetCount(item->GetCount() - 1);
	}
}

void CHARACTER::__OpenPrivateShop()
{
	if (g_bChannel > 90)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot do that on special channel."));
		return;
	}

	if (!CanOpenShop())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("YOU_DONT_MEET_SHOP_REQUIREMENTS"));
		return;
	}

	if (GetIkarusShop())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_OPEN_NEW_SHOP"));
		return;
	}


#ifdef ENABLE_OPEN_SHOP_WITH_ARMOR
	int tax = quest::CQuestManager::instance().GetEventFlag("offline_shop_tax");
	// MT2009_PLUS_SALE_TAX_V1 (open shop): the tax a sale will cost, the panels' share included.
	ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShop %d", (tax == 0 ? OFFLINESHOP_DEFAULT_TAX_PCT : tax) + playerbot_sale_tax::Percent());
#else
	unsigned bodyPart = GetPart(PART_MAIN);
	switch (bodyPart)
	{
		case 0:
		case 1:
		case 2:
			ChatPacket(CHAT_TYPE_COMMAND, "OpenPrivateShop");
			break;
		default:
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can only open the shop if you take off your armour."));
			break;
	}
#endif
}

// MYSHOP_PRICE_LIST
void CHARACTER::SendMyShopPriceListCmd(DWORD dwItemVnum, YANG dwItemPrice)
{
	char szLine[256];
	snprintf(szLine, sizeof(szLine), "MyShopPriceList %u %lld", dwItemVnum, dwItemPrice);
	ChatPacket(CHAT_TYPE_COMMAND, szLine);
	sys_log(0, szLine);
}

//

//
void CHARACTER::UseSilkBotaryReal(const TPacketMyshopPricelistHeader* p)
{
	const TItemPriceInfo* pInfo = (const TItemPriceInfo*)(p + 1);

	if (!p->byCount)

		SendMyShopPriceListCmd(1, 0);
	else {
		for (int idx = 0; idx < p->byCount; idx++)
			SendMyShopPriceListCmd(pInfo[ idx ].dwVnum, pInfo[ idx ].dwPrice);
	}

	__OpenPrivateShop();
}

//

//
void CHARACTER::UseSilkBotary(void)
{
	if (m_bNoOpenedShop) {
		DWORD dwPlayerID = GetPlayerID();
		db_clientdesc->DBPacket(HEADER_GD_MYSHOP_PRICELIST_REQ, GetDesc()->GetHandle(), &dwPlayerID, sizeof(DWORD));
		m_bNoOpenedShop = false;
	} else {
		__OpenPrivateShop();
	}
}
// END_OF_MYSHOP_PRICE_LIST

int CalculateConsume(LPCHARACTER ch)
{
	static const int WARP_NEED_LIFE_PERCENT	= 30;
	static const int WARP_MIN_LIFE_PERCENT	= 10;
	// CONSUME_LIFE_WHEN_USE_WARP_ITEM
	int consumeLife = 0;
	{
		// CheckNeedLifeForWarp
		const int curLife		= ch->GetHP();
		const int needPercent	= WARP_NEED_LIFE_PERCENT;
		const int needLife = ch->GetMaxHP() * needPercent / 100;
		if (curLife < needLife)
		{
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You don't have enough HP."));
			return -1;
		}

		consumeLife = needLife;

		const int minPercent	= WARP_MIN_LIFE_PERCENT;
		const int minLife	= ch->GetMaxHP() * minPercent / 100;
		if (curLife - needLife < minLife)
			consumeLife = curLife - minLife;

		if (consumeLife < 0)
			consumeLife = 0;
	}
	// END_OF_CONSUME_LIFE_WHEN_USE_WARP_ITEM
	return consumeLife;
}

int CalculateConsumeSP(LPCHARACTER lpChar)
{
	static const int NEED_WARP_SP_PERCENT = 30;

	const int curSP = lpChar->GetSP();
	const int needSP = lpChar->GetMaxSP() * NEED_WARP_SP_PERCENT / 100;

	if (curSP < needSP)
	{
		lpChar->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You don't have enough Spell Points (SP) to use this."));
		return -1;
	}

	return needSP;
}

// #define ENABLE_FIREWORK_STUN
#define ENABLE_ADDSTONE_FAILURE
// MT2009_PLUS_VEKIRION_V1 (chest keys), server-patches/vekirion (Autor: Vekirion):
// two PulseManager keys of their own, past the names of ePulse (the enum has
// an int underneath, so any int is a valid key and common/PulseManager.h stays
// as it is): the count of boxes opened (UseItem) and the clock of the box's
// refusal line (ITEM_GIFTBOX below).
static const ePulse MT2009_PLUS_PULSE_CHEST_OPEN = static_cast<ePulse>(0x4D540001);
static const ePulse MT2009_PLUS_PULSE_CHEST_NOTICE = static_cast<ePulse>(0x4D540002);
// Boxes a player may open in 500 ms; every other item stays at the engine's 5.
static const int MT2009_PLUS_CHEST_OPEN_PER_500MS = 60;

bool CHARACTER::UseItemEx(LPITEM item, TItemPos DestCell)
{
	// MT2009_PLUS_GOBLIN_V1 (use): the Treasure Ticket, the Goblin Key and its
	// box (playerbot_goblin.h), before the item's own limits and type.
	{
		bool GoblinUseItem(LPCHARACTER ch, LPITEM item);
		if (GoblinUseItem(this, item))
			return true;
	}
	// MT2009_PLUS_RUMI_V1 (use): the Okey card (79505) and card set (79506), playerbot_rumi.h.
	{
		bool RumiUseItem(LPCHARACTER ch, LPITEM item);
		if (RumiUseItem(this, item))
			return true;
	}
	int iLimitRealtimeStartFirstUseFlagIndex = -1;
	//int iLimitTimerBasedOnWearFlagIndex = -1;

	WORD wDestCell = DestCell.cell;
	BYTE bDestInven = DestCell.window_type;
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		long limitValue = item->GetProto()->aLimits[i].lValue;

		switch (item->GetProto()->aLimits[i].bType)
		{
			case LIMIT_LEVEL:
				if (GetLevel() < limitValue)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your level is too low to use this item."));
					return false;
				}
				break;

			case LIMIT_REAL_TIME_START_FIRST_USE:
				iLimitRealtimeStartFirstUseFlagIndex = i;
				break;

			case LIMIT_TIMER_BASED_ON_WEAR:
				//iLimitTimerBasedOnWearFlagIndex = i;
				break;
		}
	}

	if (test_server)
	{
		sys_log(0, "USE_ITEM %d %s, Inven %d, Cell %d, DestInven %d, DestCell %d, ItemType %d, SubType %d", item->GetVnum(), item->GetName(), item->GetWindow(), item->GetCell(), bDestInven, wDestCell, item->GetType(), item->GetSubType());
	}

	if ( CArenaManager::instance().IsLimitedItem( GetMapIndex(), item->GetVnum() ) == true )
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
		return false;
	}
#ifdef ENABLE_NEWSTUFF
	else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && IsLimitedPotionOnPVP(item->GetVnum()))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
		return false;
	}
#endif

	// @fixme402 (IsLoadedAffect to block affect hacking)
	if (!IsLoadedAffect())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Affects are not loaded yet!"));
		return false;
	}

	// @fixme141 BEGIN
	if (TItemPos(item->GetWindow(), item->GetCell()).IsBeltInventoryPosition())
	{
		LPITEM beltItem = GetWear(WEAR_BELT);

		if (NULL == beltItem)
		{
			ChatPacket(CHAT_TYPE_INFO, "<Belt> You can't use this item if you have no equipped belt.");
			return false;
		}

		if (false == CBeltInventoryHelper::IsAvailableCell(item->GetCell() - BELT_INVENTORY_SLOT_START, beltItem->GetValue(0)))
		{
			ChatPacket(CHAT_TYPE_INFO, "<Belt> You can't use this item if you don't upgrade your belt.");
			return false;
		}
	}
	// @fixme141 END

	if (-1 != iLimitRealtimeStartFirstUseFlagIndex)
	{
		if (0 == item->GetSocket(1))
		{
			long duration = (0 != item->GetSocket(0)) ? item->GetSocket(0) : item->GetProto()->aLimits[iLimitRealtimeStartFirstUseFlagIndex].lValue;

			if (0 == duration)
				duration = 60 * 60 * 24 * 7;

			item->SetSocket(0, time(0) + duration, true, "SOCKET_REALTIME");
			item->StartRealTimeExpireEvent();
		}

		if (false == item->IsEquipped())
			item->SetSocket(1, item->GetSocket(1) + 1, true, "SOCKET_REALTIME");
	}

	switch (item->GetType())
	{
		case ITEM_HAIR:
			return ItemProcess_Hair(item, wDestCell);

		case ITEM_POLYMORPH:
			return ItemProcess_Polymorph(item);

		case ITEM_QUEST:
#ifdef ENABLE_QUEST_DND_EVENT
			if (IS_SET(item->GetFlag(), ITEM_FLAG_APPLICABLE))
			{
				LPITEM item2;

				if (!GetItem(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				quest::CQuestManager::instance().DND(GetPlayerID(), item, item2, false);
				return true;
			}
#endif

			{
				quest::PC* questPC = quest::CQuestManager::instance().GetPCForce(GetPlayerID());
				const bool questRunning = questPC && questPC->IsRunning();
				sys_log(0, "QUEST_ITEM: use pid=%u name=%s vnum=%u flag=%u map=%ld level=%d running=%d quest=%s",
						GetPlayerID(), GetName(), item->GetVnum(), item->GetFlag(), GetMapIndex(), (int)GetLevel(),
						questRunning ? 1 : 0, questRunning ? questPC->GetCurrentQuestName().c_str() : "-");
				if (questRunning)
					ChatPacket(CHAT_TYPE_INFO, "Najpierw zamknij otwarte okno zadania (albo zaloguj sie ponownie), potem uzyj przedmiotu.");
				// Pierscien Teleportacji has one state. A __status that is not it
				// is a use MatchingQuest can neither match nor start, and nothing
				// says so; start it over.
				if (item->GetVnum() == 70058 && questPC && !questRunning)
				{
					const std::string ringQuest("teleport_ring");
					const int ringState = questPC->GetFlag(ringQuest + ".__status");
					if (ringState != 0)
					{
						sys_log(0, "QUEST_ITEM: teleport_ring state %d for pid=%u, reset to start", ringState, GetPlayerID());
						questPC->SetFlag(ringQuest + ".__status", 0);
						const unsigned int ringIndex = quest::CQuestManager::instance().GetQuestIndexByName(ringQuest);
						for (quest::PC::QuestInfoIterator qit = questPC->quest_begin(); qit != questPC->quest_end(); ++qit)
							if (qit->first == ringIndex)
								qit->second.st = 0;
					}
				}
			}
			if (GetArena() != NULL || IsObserverMode() == true)
			{
				if (item->GetVnum() == 50051 || item->GetVnum() == 50052 || item->GetVnum() == 50053)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
					return false;
				}
			}

			if (!IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_USE | ITEM_FLAG_QUEST_USE_MULTIPLE))
			{
				if (item->GetSIGVnum() == 0)
					quest::CQuestManager::instance().UseItem(GetPlayerID(), item, false);
				else
					quest::CQuestManager::instance().SIGUse(GetPlayerID(), item->GetSIGVnum(), item, false);
			}

			break;

		case ITEM_CAMPFIRE:
			{
				float fx, fy;
				GetDeltaByDegree(GetRotation(), 100.0f, &fx, &fy);

				LPSECTREE tree = SECTREE_MANAGER::instance().Get(GetMapIndex(), (long)(GetX()+fx), (long)(GetY()+fy));

				if (!tree)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot build a campfire here."));
					return false;
				}

				if (tree->IsAttr((long)(GetX()+fx), (long)(GetY()+fy), ATTR_WATER))
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot build a campfire under water."));
					return false;
				}

				LPCHARACTER campfire = CHARACTER_MANAGER::instance().SpawnMob(fishing::CAMPFIRE_MOB, GetMapIndex(), (long)(GetX()+fx), (long)(GetY()+fy), 0, false, number(0, 359));

				char_event_info* info = AllocEventInfo<char_event_info>();

				info->ch = campfire;

				campfire->m_pkMiningEvent = event_create(kill_campfire_event, info, PASSES_PER_SEC(40));

				item->SetCount(item->GetCount() - 1);
			}
			break;

		case ITEM_UNIQUE:
			{
				switch (item->GetSubType())
				{
					case USE_ABILITY_UP:
						{
							BYTE bPoint = item->GetValue(0);
							long lValue = item->GetValue(2);
							long lDuration = item->GetValue(1);
							switch (bPoint)
							{
								case POINT_MOV_SPEED:
									AddAffect(AFFECT_UNIQUE_ABILITY, bPoint, lValue, AFF_MOV_SPEED_POTION, lDuration, 0, true, true, item->GetVnum());
									break;

								case POINT_ATT_SPEED:
									AddAffect(AFFECT_UNIQUE_ABILITY, bPoint, lValue, AFF_ATT_SPEED_POTION, lDuration, 0, true, true, item->GetVnum());
									break;

								case POINT_ST:
								case POINT_DX:
								case POINT_HT:
								case POINT_IQ:
								case POINT_CASTING_SPEED:
								case POINT_RESIST_MAGIC:
								case POINT_ATT_GRADE_BONUS:
								case POINT_DEF_GRADE_BONUS:
									AddAffect(AFFECT_UNIQUE_ABILITY, bPoint, lValue, 0, lDuration, 0, true, true);
									break;
							}
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						break;

					default:
						{
							if (item->GetSubType() == USE_SPECIAL)
							{
								sys_log(0, "ITEM_UNIQUE: USE_SPECIAL %u", item->GetVnum());

								switch (item->GetVnum())
								{
									case 71049:
										if (g_bEnableBootaryCheck)
										{
											if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
											{
												UseSilkBotary();
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot open a warehouse in this area."));
											}
										}
										else
										{
											UseSilkBotary();
										}
										break;
								}
							}
							else
							{
								if (!item->IsEquipped())
									EquipItem(item);
								else
									UnequipItem(item);
							}
						}
						break;
				}
			}
			break;

		case ITEM_COSTUME:
		case ITEM_WEAPON:
		case ITEM_ARMOR:
		case ITEM_ROD:
		case ITEM_RING:
		case ITEM_BELT:
		case ITEM_HERB_KNIFE:
			// MINING
		case ITEM_PICK:
			// END_OF_MINING
			if (!item->IsEquipped())
				EquipItem(item);
			else
				UnequipItem(item);
			break;

		#ifdef ENABLE_PET_SYSTEM_EX
		case ITEM_PET:
			switch (item->GetSubType())
			{
				case PET_UPBRINGING: //1
				case PET_PAY: //12
				{
					SummonPetFromItem(item);
				}
				break;
		#endif

				case PET_EGG: //0
				case PET_BAG: //2
				case PET_FEEDSTUFF: //3
				case PET_SKILL: //4
				case PET_SKILL_DEL_BOOK: //5
				case PET_NAME_CHANGE: //6
				case PET_EXPFOOD: //7
				case PET_SKILL_ALL_DEL_BOOK: //8
				case PET_EXPFOOD_PER: //9
				case PET_ATTR_DETERMINE: //10
				case PET_ATTR_CHANGE: //11
				case PET_PRIMIUM_FEEDSTUFF: //13
				{
					// MT2009_PLUS_NEW_PET_V1 (use): the New Pet System's eggs, food,
					// elixirs and books (playerbot_newpet.h); the ItemShop seals
					// (PET_UPBRINGING, PET_PAY) stay above.
					bool NewPetUseItem(LPCHARACTER ch, LPITEM item);
					return NewPetUseItem(this, item);
				}
			}
			break;

		case ITEM_DS:
			{
				if (!item->IsEquipped())
					return false;
				return DSManager::instance().PullOut(this, NPOS, item);
			break;
			}
		case ITEM_SPECIAL_DS:
			if (!item->IsEquipped())
				EquipItem(item);
			else
				UnequipItem(item);
			break;

		case ITEM_TREASURE_BOX:
			{
				return false;

			}
			break;

		case ITEM_TREASURE_KEY:
			{
				LPITEM item2;

				if (!GetItem(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetType() != ITEM_TREASURE_BOX)
				{
					ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("This item cannot be opened with a key."));
					return false;
				}

				if (item->GetValue(0) == item2->GetValue(0))
				{
					DWORD dwBoxVnum = item2->GetVnum();
					bool bSuccess = false;
					DropSpecialItemGroup(dwBoxVnum, bSuccess);

					if (bSuccess)
					{
						item->SetCount(item->GetCount() - 1);
						item2->SetCount(item2->GetCount() - 1);

						// Szarfa (COSTUME_ACCE) tier-4 "unique" bonus drop from boss
						// chests -- operator's explicit request, 17 September 2026.
						// The box+key mechanic (ITEM_TREASURE_BOX opened only via its
						// matching ITEM_TREASURE_KEY) is this codebase's boss-chest
						// pattern, so this is the correct hook point for "wszystkie
						// szkatulki bosow" regardless of which box/key vnum pair is
						// used. Independent roll on top of the box's own
						// DropSpecialItemGroup() reward. Players only -- bots must
						// not receive it, same gate as the boss-kill tier-1 drop in
						// ITEM_MANAGER::CreateDropItem().
						// MT2009_PLUS_RARE_TOGGLE_V1 (server-patches/raretoggle): no sash from a
						// boss chest while the world has them switched off (M2_SASHES=0).
						// MT2009_PLUS_BOT_SASH_DROP_V1 (server-patches/botsashdrop): a bot's chest too.
						// MT2009_PLUS_CHEST_SASH_BOSS_ONLY_V1 (server-patches/chestsash): not from the Gold and
						// Silver Caskets (50006, 50007, 50012, 50013), only the bosses' chests.
						if (GetDesc() && dwBoxVnum != 50006 && dwBoxVnum != 50007 &&
								dwBoxVnum != 50012 && dwBoxVnum != 50013 &&
								quest::CQuestManager::instance().GetEventFlag("m2_sash_off") == 0)
						{
							// 85104 (Death Ruler Wings +3) added to the pool 17
							// September 2026, operator's explicit request, alongside
							// the classic Szarfa series -- same tier-4 shape (grade 4,
							// no further refine_set, no bonus). Chance halved the same
							// day, same request (was number(11, 19)).
							static const DWORD s_aSzarfaTier4[] = { 85004, 85008, 85014, 85018, 85024};
							const int szarfaChestChance = number(6, 10);
							int roll = number(1, 100);
							bool bSzarfaWin = roll <= szarfaChestChance;

							DWORD szarfaVnum = s_aSzarfaTier4[number(0, _countof(s_aSzarfaTier4) - 1)];

							sys_log(0, "[SZARFA_DROP] source=boss_chest box_vnum=%u opener=%s opener_pid=%u chance=%d roll=%d win=%d vnum=%u",
								dwBoxVnum, GetName(), GetPlayerID(), szarfaChestChance, roll, bSzarfaWin ? 1 : 0, szarfaVnum);

							if (bSzarfaWin)
							{
								LPITEM szarfa = ITEM_MANAGER::instance().CreateItem(szarfaVnum, 1);
								sys_log(0, "[SZARFA_DROP] source=boss_chest vnum=%u created=%d", szarfaVnum, szarfa ? 1 : 0);
								// MT2009_PLUS_BOT_SASH_DROP_V1: a bot's full bag gets nothing, never the ground.
								if (szarfa && GetDesc()->IsBot() && GetEmptyInventoryEx(szarfa) == -1)
									M2_DESTROY_ITEM(szarfa);
								else if (szarfa)
									AutoGiveItem(szarfa, true);
							}
						}
					}
					else
					{
						ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("This key does not seem to fit the lock."));
						return false;
					}
				}
				else
				{
					ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("This key does not seem to fit the lock."));
					return false;
				}
			}
			break;

		case ITEM_GIFTBOX:
			{
#ifdef ENABLE_NEWSTUFF
				if (g_BoxUseTimeLimitValue && !PulseManager::Instance().IncreaseClock(GetPlayerID(), ePulse::BoxOpening, std::chrono::milliseconds(g_BoxUseTimeLimitValue)))
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can only drop gold once in every 10 seconds."));
					return false;
				}
#endif
				DWORD dwBoxVnum = item->GetVnum();
				std::vector <DWORD> dwVnums;
				std::vector <DWORD> dwCounts;
				std::vector <LPITEM> item_gets(0);
				int count = 0;

				if( dwBoxVnum > 51500 && dwBoxVnum < 52000 )
				{
					if( !(this->DragonSoul_IsQualified()) )
					{
						// MT2009_PLUS_VEKIRION_V1 (chest notice ds): once a second,
						// not once a box - 60 refused boxes were 60 lines.
						if (PulseManager::Instance().IncreaseClock(GetPlayerID(), MT2009_PLUS_PULSE_CHEST_NOTICE, std::chrono::milliseconds(1000)))
							ChatPacket(CHAT_TYPE_INFO,LC_TEXT("Before you open the Cor Draconis, you have to complete the Dragon Stone quest and activate the Dragon Stone Alchemy."));
						return false;
					}
				}

				if (GetEmptyInventory(3) < 0)
				{
					// MT2009_PLUS_VEKIRION_V1 (chest notice room): the bag is full -
					// the box stays shut, nothing falls to the ground; the line
					// once a second, not once for each box still on its way.
					if (PulseManager::Instance().IncreaseClock(GetPlayerID(), MT2009_PLUS_PULSE_CHEST_NOTICE, std::chrono::milliseconds(1000)))
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need room for atleast 3 slot item to open the box."));
					return false;
				}

				bool bSuccess = false;
				DropSpecialItemGroup(dwBoxVnum, bSuccess);
				if (bSuccess)
				{
					item->SetCount(item->GetCount()-1);
				}
				else
				{
					ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("You have not received anything."));
					return false;
				}
			}
			break;

		case ITEM_SKILLFORGET:
			{
				if (!item->GetSocket(0))
				{
					ITEM_MANAGER::instance().RemoveItem(item);
					return false;
				}

				DWORD dwVnum = item->GetSocket(0);

				if (SkillLevelDown(dwVnum))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have lowered your Skill Level."));
				}
				else
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot lower your Skill Level."));
			}
			break;

		case ITEM_SKILLBOOK:
			{
				if (IsPolymorphed())
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read books while transformed."));
					return false;
				}

				DWORD dwVnum = 0;

				if (item->GetVnum() == 50300)
				{
					dwVnum = item->GetSocket(0);
				}
				else
				{
					dwVnum = item->GetValue(0);
				}

				if (0 == dwVnum)
				{
					ITEM_MANAGER::instance().RemoveItem(item);

					return false;
				}

				if (true == LearnSkillByBook(dwVnum))
				{
#ifdef ENABLE_BOOKS_STACKFIX
					item->SetCount(item->GetCount() - 1);
#else
					ITEM_MANAGER::instance().RemoveItem(item);
#endif
				}
			}
			break;

		case ITEM_USE:
			{
				if (item->GetVnum() > 50800 && item->GetVnum() <= 50820)
				{
					if (test_server)
						sys_log (0, "ADD addtional effect : vnum(%d) subtype(%d)", item->GetOriginalVnum(), item->GetSubType());

					int affect_type = AFFECT_EXP_BONUS_EURO_FREE;
					int apply_type = item->GetValue(0);
					int apply_value = item->GetValue(2);
					int apply_duration = item->GetValue(1);

					switch (item->GetSubType())
					{
						case USE_ABILITY_UP:
							if (FindAffect(affect_type, apply_type))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is already activated."));
								return false;
							}

							{
								switch (item->GetValue(0))
								{
									case POINT_MOV_SPEED:
										AddAffect(affect_type, apply_type, apply_value, AFF_MOV_SPEED_POTION, apply_duration, 0, true, true, item->GetVnum());
										break;

									case POINT_ATT_SPEED:
										AddAffect(affect_type, apply_type, apply_value, AFF_ATT_SPEED_POTION, apply_duration, 0, true, true, item->GetVnum());
										break;

									case POINT_ST:
									case POINT_DX:
									case POINT_HT:
									case POINT_IQ:
									case POINT_CASTING_SPEED:
									case POINT_RESIST_MAGIC:
									case POINT_ATT_GRADE_BONUS:
									case POINT_DEF_GRADE_BONUS:
										AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, true, true, item->GetVnum());
										break;
								}
							}

							if (GetDungeon())
								GetDungeon()->UsePotion(this);

							if (GetWarMap())
								GetWarMap()->UsePotion(this, item);

							item->SetCount(item->GetCount() - 1);
							break;

					case USE_AFFECT :
						{
							if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, item->GetValue(1)))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is already activated."));
							}
							else
							{
								AddAffect(AFFECT_EXP_BONUS_EURO_FREE, item->GetValue(1), item->GetValue(2), 0, item->GetValue(3), 0, false, true, item->GetVnum());
								item->SetCount(item->GetCount() - 1);
							}
						}
						break;

					case USE_POTION_NODELAY:
						{
							if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
							{
								if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
									return false;
								}

								switch (item->GetVnum())
								{
									case 70020 :
									case 71018 :
									case 71019 :
									case 71020 :
										if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
										{
											if (m_nPotionLimit <= 0)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("That is over the limit."));
												return false;
											}
										}
										break;

									default :
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
										return false;
										break;
								}
							}
#ifdef ENABLE_NEWSTUFF
							else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
								return false;
							}
#endif

							bool used = false;

							if (item->GetValue(0) != 0)
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(1) != 0)
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (item->GetValue(3) != 0)
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(4) != 0)
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (used)
							{
								if (item->GetVnum() == 50085 || item->GetVnum() == 50086)
								{
									if (test_server)
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Used Moon Cake or Seed."));
									SetUseSeedOrMoonBottleTime();
								}
								if (GetDungeon())
									GetDungeon()->UsePotion(this);

								if (GetWarMap())
									GetWarMap()->UsePotion(this, item);

								m_nPotionLimit--;

								//RESTRICT_USE_SEED_OR_MOONBOTTLE
								item->SetCount(item->GetCount() - 1);
								//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
							}
						}
						break;
					}

					return true;
				}

				if (item->GetVnum() >= 27863 && item->GetVnum() <= 27883)
				{
					if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
						return false;
					}
#ifdef ENABLE_NEWSTUFF
					else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
						return false;
					}
#endif
				}

				if (test_server)
				{
					 sys_log (0, "USE_ITEM %s Type %d SubType %d vnum %d", item->GetName(), item->GetType(), item->GetSubType(), item->GetOriginalVnum());
				}

				switch (item->GetSubType())
				{
					case USE_TIME_CHARGE_PER:
						{
							LPITEM pDestItem = GetItem(DestCell);
							if (NULL == pDestItem)
							{
								return false;
							}

							if (pDestItem->IsDragonSoul())
							{
								int ret;
								char buf[128];
								if (item->GetVnum() == DRAGON_HEART_VNUM)
								{
									ret = pDestItem->GiveMoreTime_Per((float)item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
								}
								else
								{
									ret = pDestItem->GiveMoreTime_Per((float)item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
								}
								if (ret > 0)
								{
									if (item->GetVnum() == DRAGON_HEART_VNUM)
									{
										sprintf(buf, "Inc %ds by item{VN:%d SOC%d:%ld}", ret, item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
									}
									else
									{
										sprintf(buf, "Inc %ds by item{VN:%d VAL%d:%ld}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									}

									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Charged for %d seconds"), ret);
									item->SetCount(item->GetCount() - 1);
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
									return true;
								}
								else
								{
									if (item->GetVnum() == DRAGON_HEART_VNUM)
									{
										sprintf(buf, "No change by item{VN:%d SOC%d:%ld}", item->GetVnum(), ITEM_SOCKET_CHARGING_AMOUNT_IDX, item->GetSocket(ITEM_SOCKET_CHARGING_AMOUNT_IDX));
									}
									else
									{
										sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									}

									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot charge."));
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
									return false;
								}
							}
							else
								return false;
						}
						break;
					case USE_TIME_CHARGE_FIX:
						{
							LPITEM pDestItem = GetItem(DestCell);
							if (NULL == pDestItem)
							{
								return false;
							}

							if (pDestItem->IsDragonSoul())
							{
								int ret = pDestItem->GiveMoreTime_Fix(item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
								char buf[128];
								if (ret)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Charged for %d seconds"), ret);
									sprintf(buf, "Increase %ds by item{VN:%d VAL%d:%ld}", ret, item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_SUCCESS", buf);
									item->SetCount(item->GetCount() - 1);
									return true;
								}
								else
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Cannot charge."));
									sprintf(buf, "No change by item{VN:%d VAL%d:%ld}", item->GetVnum(), ITEM_VALUE_CHARGING_AMOUNT_IDX, item->GetValue(ITEM_VALUE_CHARGING_AMOUNT_IDX));
									LogManager::instance().ItemLog(this, item, "DS_CHARGING_FAILED", buf);
									return false;
								}
							}
							else
								return false;
						}
						break;
					case USE_SPECIAL:

						switch (item->GetVnum())
						{
							// MT2009_PLUS_YUTNORI_V1 (use): the Birch Branch (79507) and the Yut Nori
							// Board (79508) go into the game's counters (playerbot_yutnori.h).
							case 79507:
							case 79508:
								{
									bool YutnoriUseItem(LPCHARACTER ch, LPITEM item);
									if (!YutnoriUseItem(this, item))
										return false;
								}
								break;
							case ITEM_NOG_POCKET:
								{
									if (FindAffect(AFFECT_NOG_ABILITY))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is already activated."));
										return false;
									}
									long time = item->GetValue(0);
									long moveSpeedPer	= item->GetValue(1);
									long attPer	= item->GetValue(2);
									long expPer			= item->GetValue(3);
									AddAffect(AFFECT_NOG_ABILITY, POINT_MOV_SPEED, moveSpeedPer, AFF_MOV_SPEED_POTION, time, 0, true, true);
									AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_ATTBONUS, attPer, AFF_NONE, time, 0, true, true);
									AddAffect(AFFECT_NOG_ABILITY, POINT_MALL_EXPBONUS, expPer, AFF_NONE, time, 0, true, true);
									item->SetCount(item->GetCount() - 1);
								}
								break;
							case ITEM_MARRIAGE_RING:
								{
									if (!IsLoadedAffect())
										return false;

									//const int delta_time = GetQuestFlag("summon_item.scroll_delay2") - get_global_time();
									//if (delta_time > 0)
									//{
									//	ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Wait %s before using the scroll again."), seconds_to_smart_time(delta_time));
									//	return false;
									//}


									marriage::TMarriage* pMarriage = marriage::CManager::instance().Get(GetPlayerID());
									if (pMarriage)
									{
										if (pMarriage->ch1 != NULL)
										{
											if (CArenaManager::instance().IsArenaMap(pMarriage->ch1->GetMapIndex()) == true)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
												break;
											}
										}

										if (pMarriage->ch2 != NULL)
										{
											if (CArenaManager::instance().IsArenaMap(pMarriage->ch2->GetMapIndex()) == true)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
												break;
											}
										}

										if (!CanWarp())
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot go elsewhere for a period of time after the trade."));
											return false;
										}

										if (HasPlayerData() && playerData->IsSummonItemDelay(this)) // notifies the player
										{
											return false;
										}

										int consumeSP = CalculateConsumeSP(this);

										if (consumeSP < 0)
											return false;

										PointChange(POINT_SP, -consumeSP, false);

										if (WarpToPID(pMarriage->GetOther(GetPlayerID())))
										{
											if (HasPlayerData())
												playerData->SetSummonItemDelay(this, 25);

											//SetQuestFlag("summon_item.scroll_delay2", get_global_time() + 15);
										}
									}
									else
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot wear a Wedding Ring if you are not married."));
								}
								break;

							case UNIQUE_ITEM_CAPE_OF_COURAGE:
							case UNIQUE_ITEM_CAPE_OF_COURAGE2:
							case 70057:
							case REWARD_BOX_UNIQUE_ITEM_CAPE_OF_COURAGE:
							{
								AggregateMonster();
								item->SetCount(item->GetCount() - 1);
							}
							break;

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
							case ACCE_REVERSAL_VNUM_1:
							case ACCE_REVERSAL_VNUM_2:
							{
								LPITEM item2;
								if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
									return false;

								if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
									return false;

								if (!CleanAcceAttr(item, item2))
									return false;
								item->SetCount(item->GetCount()-1);
								break;
							}
#endif

							case UNIQUE_ITEM_WHITE_FLAG:
							{
								if (!PulseManager::Instance().IncreaseClock(GetPlayerID(), ePulse::WhiteFlagUse, std::chrono::minutes(3)))
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("WHILE_FLAG_DELAY%d"), 3);
									return false;
								}

								ForgetMyAttacker();
								item->SetCount(item->GetCount() - 1);
							}
								break;

							case UNIQUE_ITEM_TREASURE_BOX:
								break;

							case 30093:
							case 30094:
							case 30095:
							case 30096:

								{
									const int MAX_BAG_INFO = 26;
									static struct LuckyBagInfo
									{
										DWORD count;
										int prob;
										DWORD vnum;
									} b1[MAX_BAG_INFO] =
									{
										{ 1000,	302,	1 },
										{ 10,	150,	27002 },
										{ 10,	75,	27003 },
										{ 10,	100,	27005 },
										{ 10,	50,	27006 },
										{ 10,	80,	27001 },
										{ 10,	50,	27002 },
										{ 10,	80,	27004 },
										{ 10,	50,	27005 },
										{ 1,	10,	50300 },
										{ 1,	6,	92 },
										{ 1,	2,	132 },
										{ 1,	6,	1052 },
										{ 1,	2,	1092 },
										{ 1,	6,	2082 },
										{ 1,	2,	2122 },
										{ 1,	6,	3082 },
										{ 1,	2,	3122 },
										{ 1,	6,	5052 },
										{ 1,	2,	5082 },
										{ 1,	6,	7082 },
										{ 1,	2,	7122 },
										{ 1,	1,	11282 },
										{ 1,	1,	11482 },
										{ 1,	1,	11682 },
										{ 1,	1,	11882 },
									};

									LuckyBagInfo * bi = NULL;
									bi = b1;

									int pct = number(1, 1000);

									int i;
									for (i=0;i<MAX_BAG_INFO;i++)
									{
										if (pct <= bi[i].prob)
											break;
										pct -= bi[i].prob;
									}
									if (i>=MAX_BAG_INFO)
										return false;

									if (bi[i].vnum == 50300)
									{
										GiveRandomSkillBook();
									}
									else if (bi[i].vnum == 1)
									{
										ChangeGold(1000);
									}
									else
									{
										AutoGiveItem(bi[i].vnum, bi[i].count);
									}
									ITEM_MANAGER::instance().RemoveItem(item);
								}
								break;

							case 50004:
								{
									if (item->GetSocket(0))
									{
										item->SetSocket(0, item->GetSocket(0) + 1);
									}
									else
									{
										int iMapIndex = GetMapIndex();

										PIXEL_POSITION pos;

										if (SECTREE_MANAGER::instance().GetRandomLocation(iMapIndex, pos, 700))
										{
											item->SetSocket(0, 1);
											item->SetSocket(1, pos.x);
											item->SetSocket(2, pos.y);
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use the Event Detector from this position."));
											return false;
										}
									}

									int dist = 0;
									float distance = (DISTANCE_SQRT(GetX()-item->GetSocket(1), GetY()-item->GetSocket(2)));

									if (distance < 1000.0f)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The Event Detector vanished in a mysterious light."));

										struct TEventStoneInfo
										{
											DWORD dwVnum;
											int count;
											int prob;
										};
										const int EVENT_STONE_MAX_INFO = 15;
										TEventStoneInfo info_10[EVENT_STONE_MAX_INFO] =
										{
											{ 27001, 10,  8 },
											{ 27004, 10,  6 },
											{ 27002, 10, 12 },
											{ 27005, 10, 12 },
											{ 27100,  1,  9 },
											{ 27103,  1,  9 },
											{ 27101,  1, 10 },
											{ 27104,  1, 10 },
											{ 27999,  1, 12 },

											{ 25040,  1,  4 },

											{ 27410,  1,  0 },
											{ 27600,  1,  0 },
											{ 25100,  1,  0 },

											{ 50001,  1,  0 },
											{ 50003,  1,  1 },
										};
										TEventStoneInfo info_7[EVENT_STONE_MAX_INFO] =
										{
											{ 27001, 10,  1 },
											{ 27004, 10,  1 },
											{ 27004, 10,  9 },
											{ 27005, 10,  9 },
											{ 27100,  1,  5 },
											{ 27103,  1,  5 },
											{ 27101,  1, 10 },
											{ 27104,  1, 10 },
											{ 27999,  1, 14 },

											{ 25040,  1,  5 },

											{ 27410,  1,  5 },
											{ 27600,  1,  5 },
											{ 25100,  1,  5 },

											{ 50001,  1,  0 },
											{ 50003,  1,  5 },

										};
										TEventStoneInfo info_4[EVENT_STONE_MAX_INFO] =
										{
											{ 27001, 10,  0 },
											{ 27004, 10,  0 },
											{ 27002, 10,  0 },
											{ 27005, 10,  0 },
											{ 27100,  1,  0 },
											{ 27103,  1,  0 },
											{ 27101,  1,  0 },
											{ 27104,  1,  0 },
											{ 27999,  1, 25 },

											{ 25040,  1,  0 },

											{ 27410,  1,  0 },
											{ 27600,  1,  0 },
											{ 25100,  1, 15 },

											{ 50001,  1, 10 },
											{ 50003,  1, 50 },

										};

										{
											TEventStoneInfo* info;
											if (item->GetSocket(0) <= 4)
												info = info_4;
											else if (item->GetSocket(0) <= 7)
												info = info_7;
											else
												info = info_10;

											int prob = number(1, 100);

											for (int i = 0; i < EVENT_STONE_MAX_INFO; ++i)
											{
												if (!info[i].prob)
													continue;

												if (prob <= info[i].prob)
												{
													AutoGiveItem(info[i].dwVnum, info[i].count);
													break;
												}
												prob -= info[i].prob;
											}
										}

										char chatbuf[CHAT_MAX_LEN + 1];
										int len = snprintf(chatbuf, sizeof(chatbuf), "StoneDetect %u 0 0", (DWORD)GetVID());

										if (len < 0 || len >= (int) sizeof(chatbuf))
											len = sizeof(chatbuf) - 1;

										++len;

										TPacketGCChat pack_chat;
										pack_chat.header	= HEADER_GC_CHAT;
										pack_chat.size		= sizeof(TPacketGCChat) + len;
										pack_chat.type		= CHAT_TYPE_COMMAND;
										pack_chat.id		= 0;
										pack_chat.bEmpire	= GetDesc()->GetEmpire();
										//pack_chat.id	= vid;

										TEMP_BUFFER buf;
										buf.write(&pack_chat, sizeof(TPacketGCChat));
										buf.write(chatbuf, len);

										PacketAround(buf.read_peek(), buf.size());

										ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (DETECT_EVENT_STONE) 1");
										return true;
									}
									else if (distance < 20000)
										dist = 1;
									else if (distance < 70000)
										dist = 2;
									else
										dist = 3;

									const int STONE_DETECT_MAX_TRY = 10;
									if (item->GetSocket(0) >= STONE_DETECT_MAX_TRY)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The Event Detector has vanished."));
										ITEM_MANAGER::instance().RemoveItem(item, "REMOVE (DETECT_EVENT_STONE) 0");
										AutoGiveItem(27002);
										return true;
									}

									if (dist)
									{
										char chatbuf[CHAT_MAX_LEN + 1];
										int len = snprintf(chatbuf, sizeof(chatbuf),
												"StoneDetect %u %d %d",
											   	(DWORD)GetVID(), dist, (int)GetDegreeFromPositionXY(GetX(), item->GetSocket(2), item->GetSocket(1), GetY()));

										if (len < 0 || len >= (int) sizeof(chatbuf))
											len = sizeof(chatbuf) - 1;

										++len;

										TPacketGCChat pack_chat;
										pack_chat.header	= HEADER_GC_CHAT;
										pack_chat.size		= sizeof(TPacketGCChat) + len;
										pack_chat.type		= CHAT_TYPE_COMMAND;
										pack_chat.id		= 0;
										pack_chat.bEmpire	= GetDesc()->GetEmpire();
										//pack_chat.id		= vid;

										TEMP_BUFFER buf;
										buf.write(&pack_chat, sizeof(TPacketGCChat));
										buf.write(chatbuf, len);

										PacketAround(buf.read_peek(), buf.size());
									}

								}
								break;

							case 27989:
							case 76006:
								{
									LPSECTREE_MAP pMap = SECTREE_MANAGER::instance().GetMap(GetMapIndex());

									if (pMap != NULL)
									{
										item->SetSocket(0, item->GetSocket(0) + 1);

										FFindStone f;

										// <Factor> SECTREE::for_each -> SECTREE::for_each_entity
										pMap->for_each(f);

										if (f.m_mapStone.size() > 0)
										{
											std::map<DWORD, LPCHARACTER>::iterator stone = f.m_mapStone.begin();

											DWORD max = UINT_MAX;
											LPCHARACTER pTarget = stone->second;

											while (stone != f.m_mapStone.end())
											{
												DWORD dist = (DWORD)DISTANCE_SQRT(GetX()-stone->second->GetX(), GetY()-stone->second->GetY());

												if (dist != 0 && max > dist)
												{
													max = dist;
													pTarget = stone->second;
												}
												stone++;
											}

											if (pTarget != NULL)
											{
												int val = 3;

												if (max < 10000) val = 2;
												else if (max < 70000) val = 1;

												ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u %d %d", (DWORD)GetVID(), val,
														(int)GetDegreeFromPositionXY(GetX(), pTarget->GetY(), pTarget->GetX(), GetY()));
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The detector was activated, but no spirit stones were detected.."));
											}
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The detector was activated, but no spirit stones were detected.."));
										}

										if (item->GetSocket(0) >= 6)
										{
											ChatPacket(CHAT_TYPE_COMMAND, "StoneDetect %u 0 0", (DWORD)GetVID());
											ITEM_MANAGER::instance().RemoveItem(item);
										}
									}
									break;
								}
								break;

							case 27996:
								item->SetCount(item->GetCount() - 1);
								AttackedByPoison(NULL); // @warme008
								break;

							case 27987:

								{
									item->SetCount(item->GetCount() - 1);

									int r = number(1, 100);

									if (r <= 50)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You find a simple Piece of Stone in the Clam."));
										AutoGiveItem(27990);
									}
									else
									{
										const int prob_table_gb2312[] =
										{
											95, 97, 99
										};

										const int * prob_table = prob_table_gb2312;

										if (r <= prob_table[0])
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The Clam has vanished."));
										}
										else if (r <= prob_table[1])
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is a White Pearl inside the Clam."));
											AutoGiveItem(27992);
										}
										else if (r <= prob_table[2])
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is a Blue Pearl inside the Clam."));
											AutoGiveItem(27993);
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is a Blood-Red Pearl inside the Clam."));
											AutoGiveItem(27994);
										}
									}
								}
								break;

							case 71013:
								CreateFly(number(FLY_FIREWORK1, FLY_FIREWORK6), this);
								item->SetCount(item->GetCount() - 1);
								break;

							case 50100:
							case 50101:
							case 50102:
							case 50103:
							case 50104:
							case 50105:
							case 50106:
								CreateFly(item->GetVnum() - 50100 + FLY_FIREWORK1, this);
								item->SetCount(item->GetCount() - 1);
								break;

							case 50200:
								if (g_bEnableBootaryCheck)
								{
									if (IS_BOTARYABLE_ZONE(GetMapIndex()) == true)
									{
										__OpenPrivateShop();
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot open a warehouse in this area."));
									}
								}
								else
								{
									__OpenPrivateShop();
								}
								break;

							case fishing::FISH_MIND_PILL_VNUM:
								AddAffect(AFFECT_FISH_MIND_PILL, POINT_NONE, 0, AFF_FISH_MIND, 40*60, 0, true);
								item->SetCount(item->GetCount() - 1);
								break;

							case 50301:
							case 50302:
							case 50303:
								{
									if (IsPolymorphed() == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change your status while you are transformed."));
										return false;
									}

									int lv = GetSkillLevel(SKILL_LEADERSHIP);

									if (lv < item->GetValue(0))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("It isn't easy to understand this book."));
										return false;
									}

									if (lv >= item->GetValue(1))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This book will not help you."));
										return false;
									}

									if (LearnSkillByBook(SKILL_LEADERSHIP))
									{
#ifdef ENABLE_BOOKS_STACKFIX
										item->SetCount(item->GetCount() - 1);
#else
										ITEM_MANAGER::instance().RemoveItem(item);
#endif
									}
								}
								break;

							case 50304:
							case 50305:
							case 50306:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read books while transformed."));
										return false;

									}
									if (GetSkillLevel(SKILL_COMBO) == 0 && GetLevel() < 30)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need to have a minimum level of 30 to understand this book."));
										return false;
									}

									if (GetSkillLevel(SKILL_COMBO) == 1 && GetLevel() < 50)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need a minimum level of 50 to understand this book."));
										return false;
									}

									if (GetSkillLevel(SKILL_COMBO) >= 2)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can't train any more Combos."));
										return false;
									}

									int iPct = item->GetValue(0);

									if (LearnSkillByBook(SKILL_COMBO, iPct))
									{
#ifdef ENABLE_BOOKS_STACKFIX
										item->SetCount(item->GetCount() - 1);
#else
										ITEM_MANAGER::instance().RemoveItem(item);
#endif
									}
								}
								break;
							case 50311:
							case 50312:
							case 50313:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read books while transformed."));
										return false;

									}
									DWORD dwSkillVnum = item->GetValue(0);
									int iPct = MINMAX(0, item->GetValue(1), 100);
									if (GetSkillLevel(dwSkillVnum)>=20 || dwSkillVnum-SKILL_LANGUAGE1+1 == GetEmpire())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You already understand this language."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
#ifdef ENABLE_BOOKS_STACKFIX
										item->SetCount(item->GetCount() - 1);
#else
										ITEM_MANAGER::instance().RemoveItem(item);
#endif
									}
								}
								break;

							case 50061 :
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read books while transformed."));
										return false;

									}
									DWORD dwSkillVnum = item->GetValue(0);
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetSkillLevel(dwSkillVnum) >= 10)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot train this skill."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
#ifdef ENABLE_BOOKS_STACKFIX
										item->SetCount(item->GetCount() - 1);
#else
										ITEM_MANAGER::instance().RemoveItem(item);
#endif
									}
								}
								break;

							case 50314: case 50315: case 50316:
							case 50323: case 50324:
							case 50325: case 50326:
								{
									if (IsPolymorphed() == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change your status while you are transformed."));
										return false;
									}

									int iSkillLevelLowLimit = item->GetValue(0);
									int iSkillLevelHighLimit = item->GetValue(1);
									int iPct = MINMAX(0, item->GetValue(2), 100);
									int iLevelLimit = item->GetValue(3);
									DWORD dwSkillVnum = 0;

									switch (item->GetVnum())
									{
										case 50314: case 50315: case 50316:
											dwSkillVnum = SKILL_POLYMORPH;
											break;

										case 50323: case 50324:
											dwSkillVnum = SKILL_ADD_HP;
											break;

										case 50325: case 50326:
											dwSkillVnum = SKILL_RESIST_PENETRATE;
											break;

										default:
											return false;
									}

									if (0 == dwSkillVnum)
										return false;

									if (GetLevel() < iLevelLimit)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have to improve your Level to read this Book."));
										return false;
									}

									if (GetSkillLevel(dwSkillVnum) >= 40)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot train this skill."));
										return false;
									}

									if (GetSkillLevel(dwSkillVnum) < iSkillLevelLowLimit)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("It isn't easy to understand this book."));
										return false;
									}

									if (GetSkillLevel(dwSkillVnum) >= iSkillLevelHighLimit)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot train with this Book any more."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
#ifdef ENABLE_BOOKS_STACKFIX
										item->SetCount(item->GetCount() - 1);
#else
										ITEM_MANAGER::instance().RemoveItem(item);
#endif
									}
								}
								break;

							case 50902:
							case 50903:
							case 50904:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read books while transformed."));
										return false;

									}
									DWORD dwSkillVnum = SKILL_CREATE;
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetSkillLevel(dwSkillVnum)>=40)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot train this skill."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
#ifdef ENABLE_BOOKS_STACKFIX
										item->SetCount(item->GetCount() - 1);
#else
										ITEM_MANAGER::instance().RemoveItem(item);
#endif

										if (test_server)
										{
											ChatPacket(CHAT_TYPE_INFO, "[TEST_SERVER] Success to learn skill ");
										}
									}
									else
									{
										if (test_server)
										{
											ChatPacket(CHAT_TYPE_INFO, "[TEST_SERVER] Failed to learn skill ");
										}
									}
								}
								break;

								// MINING
							case ITEM_MINING_SKILL_TRAIN_BOOK:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read books while transformed."));
										return false;

									}
									DWORD dwSkillVnum = SKILL_MINING;
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetSkillLevel(dwSkillVnum)>=40)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot train this skill."));
										return false;
									}

									if (LearnSkillByBook(dwSkillVnum, iPct))
									{
#ifdef ENABLE_BOOKS_STACKFIX
										item->SetCount(item->GetCount() - 1);
#else
										ITEM_MANAGER::instance().RemoveItem(item);
#endif
									}
								}
								break;
								// END_OF_MINING

							case ITEM_HORSE_SKILL_TRAIN_BOOK:
							case ITEM_HORSE_SKILL_TRAIN_BOOK_PLUS:
								{
									if (IsPolymorphed())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read books while transformed."));
										return false;

									}
									DWORD dwSkillVnum = SKILL_HORSE;
									int iPct = MINMAX(0, item->GetValue(1), 100);

									if (GetLevel() < 50)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need a minimum level of 50 to get riding training."));
										return false;
									}

									if (get_global_time() < M2SkillBookReadAt(this, dwSkillVnum))
									{
										if (FindAffect(AFFECT_SKILL_NO_BOOK_DELAY))
										{
											RemoveAffect(AFFECT_SKILL_NO_BOOK_DELAY);
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have escaped the evil ghost curse with the help of an Exorcism Scroll."));
										}
										else
										{
											SkillLearnWaitMoreTimeMessage(M2SkillBookReadAt(this, dwSkillVnum) - get_global_time());
											return false;
										}
									}

									if (GetPoint(POINT_HORSE_SKILL) >= 20 ||
											GetSkillLevel(SKILL_HORSE_WILDATTACK) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60 ||
											GetSkillLevel(SKILL_HORSE_WILDATTACK_RANGE) + GetSkillLevel(SKILL_HORSE_CHARGE) + GetSkillLevel(SKILL_HORSE_ESCAPE) >= 60)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot read any more Riding Guides."));
										return false;
									}

									if (number(1, 100) <= iPct)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You read the Horse Riding Manual and received a Riding Point."));
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can use this point to improve your riding skill!"));
										PointChange(POINT_HORSE_SKILL, 1);
										SetSkillNextReadTime(dwSkillVnum, get_global_time() + M2SkillBookLearnDelay(), true);
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You did not understand the riding guide."));
									}
#ifdef ENABLE_BOOKS_STACKFIX
									item->SetCount(item->GetCount() - 1);
#else
									ITEM_MANAGER::instance().RemoveItem(item);
#endif
								}
								break;

							case 70102:
							case 70103:
								{
									if (GetAlignment() >= 0)
										return false;

									int delta = MIN(-GetAlignment(), item->GetValue(0));

									sys_log(0, "%s ALIGNMENT ITEM %d", GetName(), delta);

									UpdateAlignment(delta);
									item->SetCount(item->GetCount() - 1);

									if (delta / 10 > 0)
									{
										ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("Your mind is clear. You can concentrate really well now."));
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your rank has increased by %d points."), delta/10);
									}
								}
								break;

							case 71107:
							case 39032: // @fixme169 mythical peach alternative vnum
								{
									int val = item->GetValue(0);
									int interval = item->GetValue(1);
									quest::PC* pPC = quest::CQuestManager::instance().GetPC(GetPlayerID());
									if (!pPC) // @fixme169 missing check
										return false;
									int last_use_time = pPC->GetFlag("mythical_peach.last_use_time");

									if (get_global_time() - last_use_time < interval * 60 * 60)
									{
										if (test_server == false)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use it now."));
											return false;
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Test server time limit passed"));
										}
									}

									if (GetAlignment() == 200000)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your rank cannot raise any more."));
										return false;
									}

									if (200000 - GetAlignment() < val * 10)
									{
										val = (200000 - GetAlignment()) / 10;
									}

									int old_alignment = GetAlignment() / 10;

									UpdateAlignment(val*10);

									item->SetCount(item->GetCount()-1);
									pPC->SetFlag("mythical_peach.last_use_time", get_global_time());

									ChatPacket(CHAT_TYPE_TALKING, LC_TEXT("Your mind is clear. You can concentrate really well now."));
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your rank has increased by %d points."), val);

									char buf[256 + 1];
									snprintf(buf, sizeof(buf), "%d %d", old_alignment, GetAlignment() / 10);
									LogManager::instance().CharLog(this, val, "MYTHICAL_PEACH", buf);
								}
								break;

							case 71109:
							case 72719:
								{
									LPITEM item2;

									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;

									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
										return false;

									if (item2->GetSocketCount() == 0)
										return false;

									switch( item2->GetType() )
									{
										case ITEM_WEAPON:
											break;
										case ITEM_ARMOR:
											switch (item2->GetSubType())
											{
											case ARMOR_EAR:
											case ARMOR_WRIST:
											case ARMOR_NECK:
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No metin stone to take out."));
												return false;
											}
											break;

										default:
											return false;
									}

									std::stack<long> socket;

									for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
										socket.push(item2->GetSocket(i));

									int idx = ITEM_SOCKET_MAX_NUM - 1;

									while (socket.size() > 0)
									{
										if (socket.top() > 2 && socket.top() != ITEM_BROKEN_METIN_VNUM)
											break;

										idx--;
										socket.pop();
									}

									if (socket.size() == 0)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No metin stone to take out."));
										return false;
									}

									LPITEM pItemReward = AutoGiveItem(socket.top());

									if (pItemReward != NULL)
									{
										item2->SetSocket(idx, 1, true, "SOCKET_DETACHMENT");

										char buf[256+1];
										snprintf(buf, sizeof(buf), "%s(%u) %s(%u)",
												item2->GetName(), item2->GetID(), pItemReward->GetName(), pItemReward->GetID());
										LogManager::instance().ItemLog(this, item, "USE_DETACHMENT_ONE", buf);

										item->SetCount(item->GetCount() - 1);
									}
								}
								break;

							case 70201:
							case 70202:
							case 70203:
							case 70204:
							case 70205:
							case 70206:
								{
									// NEW_HAIR_STYLE_ADD
									if (GetPart(PART_HAIR) >= 1001)
									{
										if (item->GetVnum() != 70201)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot dye or bleach your current hairstyle."));
										}
										else
										{
											if (UnequipItem(GetWear(WEAR_COSTUME_HAIR), item))
												item->SetCount(item->GetCount() - 1);
										}
									}
									// END_NEW_HAIR_STYLE_ADD
									else
									{
										quest::CQuestManager& q = quest::CQuestManager::instance();
										quest::PC* pPC = q.GetPC(GetPlayerID());

										if (pPC)
										{
											int last_dye_level = pPC->GetFlag("dyeing_hair.last_dye_level");

											if (last_dye_level == 0 ||
													last_dye_level+3 <= GetLevel() ||
													item->GetVnum() == 70201)
											{
												SetPart(PART_HAIR, item->GetVnum() - 70201);

												if (item->GetVnum() == 70201)
													pPC->SetFlag("dyeing_hair.last_dye_level", 0);
												else
													pPC->SetFlag("dyeing_hair.last_dye_level", GetLevel());

												item->SetCount(item->GetCount() - 1);
												UpdatePacket();
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need to have reached level %d to be able to dye your hair again."), last_dye_level+3);
											}
										}
									}
								}
								break;

							case ITEM_NEW_YEAR_GREETING_VNUM:
								{
									DWORD dwBoxVnum = ITEM_NEW_YEAR_GREETING_VNUM;
									bool bSuccess = false;
									DropSpecialItemGroup(dwBoxVnum, bSuccess);
									if (bSuccess)
									{
										item->SetCount(item->GetCount() - 1);
									}
								}
								break;

							case ITEM_VALENTINE_ROSE:
							case ITEM_VALENTINE_CHOCOLATE:
								{
									DWORD dwBoxVnum = item->GetVnum();

									if (((item->GetVnum() == ITEM_VALENTINE_ROSE) && (SEX_MALE==GET_SEX(this))) ||
										((item->GetVnum() == ITEM_VALENTINE_CHOCOLATE) && (SEX_FEMALE==GET_SEX(this))))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item can only be opened by the another gender."));
										return false;
									}

									bool bSuccess = false;
									DropSpecialItemGroup(dwBoxVnum, bSuccess);
									if (bSuccess)
										item->SetCount(item->GetCount() - 1);
								}
								break;

							case ITEM_WHITEDAY_CANDY:
							case ITEM_WHITEDAY_ROSE:
								{
									DWORD dwBoxVnum = item->GetVnum();

									if (((item->GetVnum() == ITEM_WHITEDAY_CANDY) && (SEX_MALE==GET_SEX(this))) ||
										((item->GetVnum() == ITEM_WHITEDAY_ROSE) && (SEX_FEMALE==GET_SEX(this))))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item can only be opened by the another gender."));
										return false;
									}

									bool bSuccess = false;
									DropSpecialItemGroup(dwBoxVnum, bSuccess);
									if (bSuccess)
										item->SetCount(item->GetCount() - 1);
								}
								break;

							case ITEM_GIVE_STAT_RESET_COUNT_VNUM:
								{
									PointChange(POINT_STAT_RESET_COUNT, 1);
									item->SetCount(item->GetCount()-1);
								}
								break;

							case 50107:
								{
									if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
										return false;
									}
#ifdef ENABLE_NEWSTUFF
									else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
										return false;
									}
#endif

									EffectPacket(SE_CHINA_FIREWORK);
#ifdef ENABLE_FIREWORK_STUN

									AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5*60, 0, true);
#endif
									item->SetCount(item->GetCount()-1);
								}
								break;

							case 50108:
								{
									if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
										return false;
									}
#ifdef ENABLE_NEWSTUFF
									else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
										return false;
									}
#endif

									EffectPacket(SE_SPIN_TOP);
#ifdef ENABLE_FIREWORK_STUN

									AddAffect(AFFECT_CHINA_FIREWORK, POINT_STUN_PCT, 30, AFF_CHINA_FIREWORK, 5*60, 0, true);
#endif
									item->SetCount(item->GetCount()-1);
								}
								break;

							case ITEM_WONSO_BEAN_VNUM:
								PointChange(POINT_HP, GetMaxHP() - GetHP());
								item->SetCount(item->GetCount()-1);
								break;

							case ITEM_WONSO_SUGAR_VNUM:
								PointChange(POINT_SP, GetMaxSP() - GetSP());
								item->SetCount(item->GetCount()-1);
								break;

							case ITEM_WONSO_FRUIT_VNUM:
								PointChange(POINT_STAMINA, GetMaxStamina()-GetStamina());
								item->SetCount(item->GetCount()-1);
								break;

							case ITEM_ELK_VNUM:
								{
									YANG iGold = item->GetSocket(0);
									ITEM_MANAGER::instance().RemoveItem(item);
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have received %d Yang."), iGold);
									ChangeGold(iGold);
								}
								break;

							case 70021:
								{
									YANG HealPrice = quest::CQuestManager::instance().GetEventFlag("MonarchHealGold");
									if (HealPrice == 0)
										HealPrice = 2000000;

									if (CMonarch::instance().HealMyEmpire(this, HealPrice))
									{
										char szNotice[256];
										snprintf(szNotice, sizeof(szNotice), LC_TEXT("When the Blessing of the Emperor is used %s the HP and SP are restored again."), EMPIRE_NAME(GetEmpire()));
										SendNoticeMap(szNotice, GetMapIndex(), false);

										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The Emperor Blessing is activated."));
									}
								}
								break;

							case 27995:
								{
								}
								break;

							case 71092 :
								{
									if (m_pkChrTarget != NULL)
									{
										if (m_pkChrTarget->IsPolymorphed())
										{
											m_pkChrTarget->SetPolymorph(0);
											m_pkChrTarget->RemoveAffect(AFFECT_POLYMORPH);
										}
									}
									else
									{
										if (IsPolymorphed())
										{
											SetPolymorph(0);
											RemoveAffect(AFFECT_POLYMORPH);
										}
									}
								}
								break;

							case 71051 :
								{
									LPITEM item2;

									if (!IsValidItemPosition(DestCell) || !(item2 = GetInventoryItem(wDestCell)))
										return false;

									if (ITEM_COSTUME == item2->GetType()) // @fixme124
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
										return false;
									}

									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
										return false;

									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
										return false;
									}

#ifdef ENABLE_ITEM_RARE_ATTR_LEVEL_PCT
									if (item2->AddRareAttribute2())
#else
									if (item2->AddRareAttribute())
#endif
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("New bonus added successfully."));

										int iAddedIdx = item2->GetRareAttrCount() + 4;
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										LogManager::instance().ItemLog(
												GetPlayerID(),
												item2->GetAttributeType(iAddedIdx),
												item2->GetAttributeValue(iAddedIdx),
												item->GetID(),
												"ADD_RARE_ATTR",
												buf,
												GetDesc()->GetHostName(),
												item->GetOriginalVnum());

										item->SetCount(item->GetCount() - 1);
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot add more bonus."));
									}
								}
								break;

							case 71052 :
								{
									LPITEM item2;

									if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
										return false;

									if (ITEM_COSTUME == item2->GetType()) // @fixme124
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
										return false;
									}

									if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
										return false;

									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
										return false;
									}

#ifdef ENABLE_ITEM_RARE_ATTR_LEVEL_PCT
									if (item2->ChangeRareAttribute2())
#else
									if (item2->ChangeRareAttribute())
#endif
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());
										LogManager::instance().ItemLog(this, item, "CHANGE_RARE_ATTR", buf);

										item->SetCount(item->GetCount() - 1);
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the bonuses."));
									}
								}
								break;

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
								{
									if (!PulseManager::Instance().IncreaseClock(GetPlayerID(), ePulse::AutoPotionUse, std::chrono::milliseconds(1000)))
										return false;

									if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
										return false;
									}
#ifdef ENABLE_NEWSTUFF
									else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
										return false;
									}
#endif

									EAffectTypes type = AFFECT_NONE;
									bool isSpecialPotion = false;

									switch (item->GetVnum())
									{
										case ITEM_AUTO_HP_RECOVERY_X:
											isSpecialPotion = true;

										case ITEM_AUTO_HP_RECOVERY_S:
										case ITEM_AUTO_HP_RECOVERY_M:
										case ITEM_AUTO_HP_RECOVERY_L:
										case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_XS:
										case REWARD_BOX_ITEM_AUTO_HP_RECOVERY_S:
										case FUCKING_BRAZIL_ITEM_AUTO_HP_RECOVERY_S:
											type = AFFECT_AUTO_HP_RECOVERY;
											break;

										case ITEM_AUTO_SP_RECOVERY_X:
											isSpecialPotion = true;

										case ITEM_AUTO_SP_RECOVERY_S:
										case ITEM_AUTO_SP_RECOVERY_M:
										case ITEM_AUTO_SP_RECOVERY_L:
										case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_XS:
										case REWARD_BOX_ITEM_AUTO_SP_RECOVERY_S:
										case FUCKING_BRAZIL_ITEM_AUTO_SP_RECOVERY_S:
											type = AFFECT_AUTO_SP_RECOVERY;
											break;
									}

									if (AFFECT_NONE == type)
										break;

									if (item->GetCount() > 1)
									{
										int pos = GetEmptyInventory(item->GetSize());

										if (-1 == pos)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There isn't enough space in your inventory."));
											break;
										}

										item->SetCount( item->GetCount() - 1 );

										LPITEM item2 = ITEM_MANAGER::instance().CreateItem( item->GetVnum(), 1 );
										item2->AddToCharacter(this, TItemPos(INVENTORY, pos));

										if (item->GetSocket(1) != 0)
										{
											item2->SetSocket(1, item->GetSocket(1));
										}

										item = item2;
									}

									CAffect* pAffect = FindAffect( type );

									if (NULL == pAffect)
									{
										if (item->GetSocket(1) == item->GetSocket(2))
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("AUTOPOTION_IS_EMPTY"));
											break;
										}

										EPointTypes bonus = POINT_NONE;

										AddAffect( type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);

										item->Lock(true);
										item->SetSocket(0, true, true, "SOCKET_AUTOPOTION");

										AutoRecoveryItemProcess( type );
									}
									else
									{
										if (item->GetID() == pAffect->dwFlag)
										{
											RemoveAffect( pAffect );

											item->Lock(false);
											item->SetSocket(0, false, true, "SOCKET_AUTOPOTION");
										}
										else
										{
											LPITEM old = FindItemByID( pAffect->dwFlag );

											if (NULL != old)
											{
												old->Lock(false);
												old->SetSocket(0, false, true, "SOCKET_AUTOPOTION");
											}

											RemoveAffect( pAffect );

											EPointTypes bonus = POINT_NONE;

											if (true == isSpecialPotion)
											{
												if (type == AFFECT_AUTO_HP_RECOVERY)
												{
													bonus = POINT_MAX_HP_PCT;
												}
												else if (type == AFFECT_AUTO_SP_RECOVERY)
												{
													bonus = POINT_MAX_SP_PCT;
												}
											}

											AddAffect( type, bonus, 4, item->GetID(), INFINITE_AFFECT_DURATION, 0, true, false);

											item->Lock(true);
											item->SetSocket(0, true, true, "SOCKET_AUTOPOTION");

											AutoRecoveryItemProcess( type );
										}
									}
								}
								break;
						}
						break;

					case USE_CLEAR:
						{
							switch (item->GetVnum())
							{
#ifdef ENABLE_WOLFMAN_CHARACTER
								case 27124: // Bandage
									RemoveBleeding();
									break;
#endif
								case 27874: // Grilled Perch
								default:
									RemoveBadAffect();
									break;
							}
							item->SetCount(item->GetCount() - 1);
						}
						break;

					case USE_INVISIBILITY:
						{
							if (item->GetVnum() == 70026)
							{
								quest::CQuestManager& q = quest::CQuestManager::instance();
								quest::PC* pPC = q.GetPC(GetPlayerID());

								if (pPC != NULL)
								{
									int last_use_time = pPC->GetFlag("mirror_of_disapper.last_use_time");

									if (get_global_time() - last_use_time < 10*60)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use it now."));
										return false;
									}

									pPC->SetFlag("mirror_of_disapper.last_use_time", get_global_time());
								}
							}

							AddAffect(AFFECT_INVISIBILITY, POINT_NONE, 0, AFF_INVISIBILITY, 300, 0, true);
							item->SetCount(item->GetCount() - 1);
						}
						break;

					case USE_POTION_NODELAY:
						{
							if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
							{
								if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
									return false;
								}

								switch (item->GetVnum())
								{
									case 70020 :
									case 71018 :
									case 71019 :
									case 71020 :
										if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
										{
											if (m_nPotionLimit <= 0)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("That is over the limit."));
												return false;
											}
										}
										break;

									default :
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
										return false;
								}
							}
#ifdef ENABLE_NEWSTUFF
							else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
								return false;
							}
#endif

							bool used = false;

							if (item->GetValue(0) != 0)
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(0) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(1) != 0)
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(1) * (100 + GetPoint(POINT_POTION_BONUS)) / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (item->GetValue(3) != 0)
							{
								if (GetHP() < GetMaxHP())
								{
									PointChange(POINT_HP, item->GetValue(3) * GetMaxHP() / 100);
									EffectPacket(SE_HPUP_RED);
									used = TRUE;
								}
							}

							if (item->GetValue(4) != 0)
							{
								if (GetSP() < GetMaxSP())
								{
									PointChange(POINT_SP, item->GetValue(4) * GetMaxSP() / 100);
									EffectPacket(SE_SPUP_BLUE);
									used = TRUE;
								}
							}

							if (used)
							{
								if (item->GetVnum() == 50085 || item->GetVnum() == 50086)
								{
									if (test_server)
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Used Moon Cake or Seed."));
									SetUseSeedOrMoonBottleTime();
								}
								if (GetDungeon())
									GetDungeon()->UsePotion(this);

								if (GetWarMap())
									GetWarMap()->UsePotion(this, item);

								m_nPotionLimit--;

								//RESTRICT_USE_SEED_OR_MOONBOTTLE
								item->SetCount(item->GetCount() - 1);
								//END_RESTRICT_USE_SEED_OR_MOONBOTTLE
							}
						}
						break;

					case USE_POTION:
						if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
						{
							if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit") > 0)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
								return false;
							}

							switch (item->GetVnum())
							{
								case 27001 :
								case 27002 :
								case 27003 :
								case 27004 :
								case 27005 :
								case 27006 :
									if (quest::CQuestManager::instance().GetEventFlag("arena_potion_limit_count") < 10000)
									{
										if (m_nPotionLimit <= 0)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("That is over the limit."));
											return false;
										}
									}
									break;

								default :
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this in the duel arena."));
									return false;
							}
						}
#ifdef ENABLE_NEWSTUFF
						else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
						{
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
							return false;
						}
#endif

						if (item->GetValue(1) != 0)
						{
							if (GetPoint(POINT_SP_RECOVERY) + GetSP() >= GetMaxSP())
							{
								return false;
							}

							PointChange(POINT_SP_RECOVERY, item->GetValue(1) * MIN(200, (100 + GetPoint(POINT_SP_REGEN))) / 100);
							StartAffectEvent();
							EffectPacket(SE_SPUP_BLUE);
						}

						if (item->GetValue(0) != 0)
						{
							if (GetPoint(POINT_HP_RECOVERY) + GetHP() >= GetMaxHP())
							{
								return false;
							}

							PointChange(POINT_HP_RECOVERY, item->GetValue(0) * MIN(200, (100 + GetPoint(POINT_HP_REGEN))) / 100);
							StartAffectEvent();
							EffectPacket(SE_HPUP_RED);
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						m_nPotionLimit--;
						break;

					case USE_POTION_CONTINUE:
						{
							if (item->GetValue(0) != 0)
							{
								AddAffect(AFFECT_HP_RECOVER_CONTINUE, POINT_HP_RECOVER_CONTINUE, item->GetValue(0), 0, item->GetValue(2), 0, true);
							}
							else if (item->GetValue(1) != 0)
							{
								AddAffect(AFFECT_SP_RECOVER_CONTINUE, POINT_SP_RECOVER_CONTINUE, item->GetValue(1), 0, item->GetValue(2), 0, true);
							}
							else
								return false;
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						break;

					case USE_ABILITY_UP:
						{
							BYTE bPoint = item->GetValue(0);
							switch (bPoint)
							{
								case POINT_MOV_SPEED:
									AddAffect(AFFECT_MOV_SPEED, POINT_MOV_SPEED, item->GetValue(2), AFF_MOV_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
									EffectPacket(SE_DXUP_PURPLE);
#endif
									break;

								case POINT_ATT_SPEED:
									AddAffect(AFFECT_ATT_SPEED, POINT_ATT_SPEED, item->GetValue(2), AFF_ATT_SPEED_POTION, item->GetValue(1), 0, true);
#ifdef ENABLE_EFFECT_EXTRAPOT
									EffectPacket(SE_SPEEDUP_GREEN);
#endif
									break;

								case POINT_ST:
								case POINT_DX:
								case POINT_HT:
								case POINT_IQ:
								case POINT_CASTING_SPEED:
								case POINT_ATT_GRADE_BONUS:
								case POINT_DEF_GRADE_BONUS:
									AddAffect(AFFECT_STR, bPoint, item->GetValue(2), 0, item->GetValue(1), 0, true);
							}
						}

						if (GetDungeon())
							GetDungeon()->UsePotion(this);

						if (GetWarMap())
							GetWarMap()->UsePotion(this, item);

						item->SetCount(item->GetCount() - 1);
						break;

					case USE_TALISMAN:
						{
							if (!IsLoadedAffect())
								return false;

							const int TOWN_PORTAL	= 1;
							const int MEMORY_PORTAL = 2;

							if (GetMapIndex() == 200 || GetMapIndex() == 113)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this from your current position."));
								return false;
							}

							if (CArenaManager::instance().IsArenaMap(GetMapIndex()) == true)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
								return false;
							}
#ifdef ENABLE_NEWSTUFF
							else if (g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(item->GetVnum()))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item in a duel."));
								return false;
							}
#endif

							if (IsWarping())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are ready to warp, so you cannot use the Scroll of the Location."));
								return false;
							}

							if (item->GetSocket(0) != 0)
							{
								if (!CanWarp())
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("YOU_CANNOT_WARP_NOW"));
									return false;
								}

								if (HasPlayerData() && playerData->IsSummonItemDelay(this)) // notifies the player
								{
									return false;
								}
							}

							if (item->GetValue(0) == TOWN_PORTAL || item->GetSocket(0) != 0)
							{
								const int delta_time = GetQuestFlag("summon_item.scroll_delay") - get_global_time();
								if (delta_time > 0)
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Wait %s before using the scroll again."), seconds_to_smart_time(delta_time));
									return false;
								}
							}

							if (item->GetValue(0) == TOWN_PORTAL)
							{
								if (item->GetSocket(0) == 0)
								{
									if (!GetDungeon())
										if (!GiveRecallItem(item))
											return false;

									PIXEL_POSITION posWarp;
									BYTE bEmpire = GetEmpire();
									posWarp.x = g_start_position[bEmpire][0];
									posWarp.y = g_start_position[bEmpire][1];

									if (HasPlayerData())
									{
										playerData->SetSummonItemDelay(this, 20);
									}

									WarpSet(posWarp.x, posWarp.y);
								}
								else
								{
									if (test_server)
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are being brought back to the place of origin."));

									ProcessRecallItem(item);
								}
							}
							else if (item->GetValue(0) == MEMORY_PORTAL)
							{
								if (item->GetSocket(0) == 0)
								{
									if (GetDungeon())
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s%s cannot be used in a dungeon."),
												item->GetName(),
												"");
										return false;
									}

									if (!GiveRecallItem(item))
										return false;
								}
								else
								{
									ProcessRecallItem(item);
								}
							}
						}
						break;

					case USE_TUNING:
					case USE_DETACHMENT:
						{
							LPITEM item2;

							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
								return false;

							if (item2->GetVnum() >= 28330 && item2->GetVnum() <= 28343)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("+3 spirit stones can not be upgraded by this item."));
								return false;
							}

#ifdef ENABLE_ACCE_COSTUME_SYSTEM
							if (item->GetValue(0) == ACCE_CLEAN_ATTR_VALUE0
								|| item->GetVnum() == ACCE_REVERSAL_VNUM_1
								|| item->GetVnum() == ACCE_REVERSAL_VNUM_2
							)
							{
								if (!CleanAcceAttr(item, item2))
									return false;
								item->SetCount(item->GetCount()-1);
								return true;
							}
#endif

							if (item2->GetVnum() >= 28430 && item2->GetVnum() <= 28443)
							{
								if (item->GetVnum() == 71056)
								{
									RefineItem(item, item2);
								}
								else
								{
									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The spirit stone can not be advanced by this item."));
								}
							}
							else
							{
								RefineItem(item, item2);
							}
						}
						break;

					case USE_CHANGE_COSTUME_ATTR:
					case USE_RESET_COSTUME_ATTR:
						{
							// GF26 costume bonus items (custom-patches/gf26_costume_bonus):
							// 70063 USE_RESET_COSTUME_ATTR rolls 1-3 bonuses from scratch (also on a
							// costume with none), 70064 USE_CHANGE_COSTUME_ATTR re-rolls the values
							// and keeps the count. Only BODY/HAIR/WEAPON costumes in the ordinary
							// inventory; the item is used up only after a successful roll, and a
							// roll that went wrong puts the old bonuses back.
							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (!DestCell.IsDefaultInventoryPosition() || ITEM_COSTUME != item2->GetType()
								|| (item2->GetSubType() != COSTUME_BODY && item2->GetSubType() != COSTUME_HAIR
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
									&& item2->GetSubType() != COSTUME_WEAPON
#endif
								))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
								return false;
							}

							if (item2->IsExchanging() || item2->IsEquipped() || item2->isLocked() || !CanHandleItem()) // @fixme114
								return false;

							if (item2->GetAttributeSetIndex() == -1)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
								return false;
							}

							const int iCountBefore = item2->GetAttributeCount();
							if (item->GetSubType() == USE_CHANGE_COSTUME_ATTR && iCountBefore == 0)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is no upgrade that you can change."));
								return false;
							}

							TPlayerItemAttribute aBefore[ITEM_ATTRIBUTE_MAX_NUM];
							memcpy(aBefore, item2->GetAttributes(), sizeof(aBefore));
							const char* how = item->GetSubType() == USE_CHANGE_COSTUME_ATTR ? "CHANGE_COSTUME_ATTR" : "RESET_COSTUME_ATTR";
							char hint[64];
							snprintf(hint, sizeof(hint), "reagent %u id %u vnum %u", item->GetVnum(), item2->GetID(), item2->GetVnum());
							LogManager::instance().ItemLog(this, item2, (std::string(how) + "_BEFORE").c_str(), hint, true);

							if (item->GetSubType() == USE_CHANGE_COSTUME_ATTR)
								item2->ChangeAttribute();
							else
							{
								item2->ClearAttribute();
								item2->AlterToMagicItem();
							}

							const int iCountAfter = item2->GetAttributeCount();
							const bool bOk = item->GetSubType() == USE_CHANGE_COSTUME_ATTR
								? iCountAfter == iCountBefore
								: (iCountAfter >= 1 && iCountAfter <= 3);
							if (!bOk)
							{
								item2->SetAttributes(aBefore);
								item2->UpdatePacket();
								LogManager::instance().ItemLog(this, item2, (std::string(how) + "_FAILED").c_str(), hint, true);
								sys_err("%s: %s pid %u costume id %u vnum %u rolled %d bonus(es) (had %d) - restored, reagent kept",
									how, GetName(), GetPlayerID(), item2->GetID(), item2->GetVnum(), iCountAfter, iCountBefore);
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
								return false;
							}

							item2->UpdatePacket();
							LogManager::instance().ItemLog(this, item2, (std::string(how) + "_AFTER").c_str(), hint, true);
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have changed the upgrade."));

							item->SetCount(item->GetCount() - 1);
							break;
						}

						//  ACCESSORY_REFINE & ADD/CHANGE_ATTRIBUTES
					case USE_PUT_INTO_BELT_SOCKET:
					case USE_PUT_INTO_RING_SOCKET:
					case USE_PUT_INTO_ACCESSORY_SOCKET:
					case USE_ADD_ACCESSORY_SOCKET:
					case USE_CLEAN_SOCKET:
					case USE_CHANGE_ATTRIBUTE:
					case USE_CHANGE_ATTRIBUTE2 :
					case USE_ADD_ATTRIBUTE:
					case USE_ADD_ATTRIBUTE2:
						{
							LPITEM item2;
							if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
								return false;

							if (ITEM_COSTUME == item2->GetType())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
								return false;
							}

							if (item2->GetVnum() >= 11901 && item2->GetVnum() <= 11904 || item2->GetVnum() == 50201 || item2->GetVnum() == 50202) // wedding items
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
								return false;
							}

							if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
								return false;

							switch (item->GetSubType())
							{
								case USE_CLEAN_SOCKET:
									{
										int max_clean_count = item->GetValue(0);
										int clean_count = 0;
										int shift_count = 0;

										int i;
										for (i = 0; i < METIN_SOCKET_MAX_NUM; ++i)
										{
											long socket = item2->GetSocket(i);
											if (clean_count < max_clean_count && socket == ITEM_BROKEN_METIN_VNUM)
											{
												item2->SetSocket(i, 1, true, "SOCKET_CLEAN");
												clean_count++;
											}
											else if (socket > 0
												&& clean_count > 0 && i >= clean_count)
											{
												shift_count = MIN(clean_count, i);
											}
										}

										if (clean_count == 0)
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There aren't any Pieces of Broken Stone available for removal."));
											return false;
										}

										int last_socket_index = METIN_SOCKET_MAX_NUM - 1;
										for (int j = 0; j < METIN_SOCKET_MAX_NUM; j++)
										{
											if (item2->GetSocket(j) == 0)
												break;

											if (item2->GetSocket(j) > 1)
												continue;

											for (int k = j + 1; k < METIN_SOCKET_MAX_NUM; k++)
											{
												long next_socket = item2->GetSocket(k);
												if (next_socket > 1)
												{
													item2->SetSocket(j, next_socket, true, "SOCKET_CLEAN");
													item2->SetSocket(k, 1, true, "SOCKET_CLEAN");
													break;
												}
											}
										}

										{
											char buf[21];
											snprintf(buf, sizeof(buf), "%u", item2->GetID());
											LogManager::instance().ItemLog(this, item, "CLEAN_SOCKET", buf);
										}

										item->SetCount(item->GetCount() - 1);

									}
									break;

								case USE_CHANGE_ATTRIBUTE :
								case USE_CHANGE_ATTRIBUTE2 : // @fixme123
									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
										return false;
									}

									if (item2->GetAttributeCount() == 0)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There is no upgrade that you can change."));
										return false;
									}

									if (item->GetSubType() == USE_CHANGE_ATTRIBUTE2)
									{
										int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
										{
											0, 0, 30, 40, 3
										};

										item2->ChangeAttribute(aiChangeProb);
									}
									else if (item->GetVnum() == 76014)
									{
										int aiChangeProb[ITEM_ATTRIBUTE_MAX_LEVEL] =
										{
											0, 10, 50, 39, 1
										};

										item2->ChangeAttribute(aiChangeProb);
									}

									else
									{
										if (item->GetVnum() == 71151 || item->GetVnum() == 76023)
										{
											if ((item2->GetType() == ITEM_WEAPON)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY))
											{
												bool bCanUse = true;
												for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
												{
													if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
													{
														bCanUse = false;
														break;
													}
												}
												if (false == bCanUse)
												{
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item can only be used on weapons or armor up to level 40."));
													break;
												}
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be used on accesories."));
												break;
											}
										}
										item2->ChangeAttribute();
									}

									ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have changed the upgrade."));
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());
										LogManager::instance().ItemLog(this, item, "CHANGE_ATTRIBUTE", buf);
									}

									item->SetCount(item->GetCount() - 1);
									break;

								case USE_ADD_ATTRIBUTE :
									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
										return false;
									}

									if (item2->GetAttributeCount() < 4)
									{
										if (item->GetVnum() == 71152 || item->GetVnum() == 76024)
										{
											if ((item2->GetType() == ITEM_WEAPON)
												|| (item2->GetType() == ITEM_ARMOR && item2->GetSubType() == ARMOR_BODY))
											{
												bool bCanUse = true;
												for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
												{
													if (item2->GetLimitType(i) == LIMIT_LEVEL && item2->GetLimitValue(i) > 40)
													{
														bCanUse = false;
														break;
													}
												}
												if (false == bCanUse)
												{
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item can only be used on weapons or armor up to level 40."));
													break;
												}
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be used on accesories."));
												break;
											}
										}
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
										{
											item2->AddAttribute();
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Upgrade successfully added."));

											int iAddedIdx = item2->GetAttributeCount() - 1;
											LogManager::instance().ItemLog(
													GetPlayerID(),
													item2->GetAttributeType(iAddedIdx),
													item2->GetAttributeValue(iAddedIdx),
													item->GetID(),
													"ADD_ATTRIBUTE_SUCCESS",
													buf,
													GetDesc()->GetHostName(),
													item->GetOriginalVnum());
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No upgrade added."));
											LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE_FAIL", buf);
										}

										item->SetCount(item->GetCount() - 1);
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You must use the Blessing Marble in order to add another bonus to this item."));
									}
									break;

								case USE_ADD_ATTRIBUTE2 :

									if (item2->GetAttributeSetIndex() == -1)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the upgrade of this item."));
										return false;
									}

									if (item2->GetAttributeCount() == 4)
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (number(1, 100) <= aiItemAttributeAddPercent[item2->GetAttributeCount()])
										{
											item2->AddAttribute();
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Upgrade successfully added."));

											int iAddedIdx = item2->GetAttributeCount() - 1;
											LogManager::instance().ItemLog(
													GetPlayerID(),
													item2->GetAttributeType(iAddedIdx),
													item2->GetAttributeValue(iAddedIdx),
													item->GetID(),
													"ADD_ATTRIBUTE2_SUCCESS",
													buf,
													GetDesc()->GetHostName(),
													item->GetOriginalVnum());
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No upgrade added."));
											LogManager::instance().ItemLog(this, item, "ADD_ATTRIBUTE2_FAIL", buf);
										}

										item->SetCount(item->GetCount() - 1);
									}
									else if (item2->GetAttributeCount() == 5)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item can no longer be improved. The maximum number of bonuses has been reached."));
									}
									else if (item2->GetAttributeCount() < 4)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can only use the Blessing Marble with an item which already has 4 bonuses."));
									}
									else
									{
										// wtf ?!
										sys_err("ADD_ATTRIBUTE2 : Item has wrong AttributeCount(%d)", item2->GetAttributeCount());
									}
									break;

								case USE_ADD_ACCESSORY_SOCKET:
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (item2->IsAccessoryForSocket())
										{
											if (item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
											{
#ifdef ENABLE_ADDSTONE_FAILURE
												if (number(1, 100) <= 50)
#else
												if (1)
#endif
												{
													item2->SetAccessorySocketMaxGrade(item2->GetAccessorySocketMaxGrade() + 1);
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Socket successfully added."));
													LogManager::instance().ItemLog(this, item, "ADD_SOCKET_SUCCESS", buf);
												}
												else
												{
													ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No socket added."));
													LogManager::instance().ItemLog(this, item, "ADD_SOCKET_FAIL", buf);
												}

												item->SetCount(item->GetCount() - 1);
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No additional sockets could be added to this item."));
											}
										}
										else
										{
											ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot add a socket to this item."));
										}
									}
									break;

								case USE_PUT_INTO_BELT_SOCKET:
								case USE_PUT_INTO_ACCESSORY_SOCKET:
									if (item2->IsAccessoryForSocket() && item->CanPutInto(item2))
									{
										char buf[21];
										snprintf(buf, sizeof(buf), "%u", item2->GetID());

										if (item2->GetAccessorySocketGrade() < item2->GetAccessorySocketMaxGrade())
										{
											if (number(1, 100) <= aiAccessorySocketPutPct[item2->GetAccessorySocketGrade()])
											{
												item2->SetAccessorySocketGrade(item2->GetAccessorySocketGrade() + 1);
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Arming successful."));
												LogManager::instance().ItemLog(this, item, "PUT_SOCKET_SUCCESS", buf);
											}
											else
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Arming has failed."));
												LogManager::instance().ItemLog(this, item, "PUT_SOCKET_FAIL", buf);
											}

											item->SetCount(item->GetCount() - 1);
										}
										else
										{
											if (item2->GetAccessorySocketMaxGrade() == 0)
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have to add a socket first. Use a diamond in order to do this."));
											else if (item2->GetAccessorySocketMaxGrade() < ITEM_ACCESSORY_SOCKET_MAX_NUM)
											{
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("There are no sockets for gemstones in this item."));
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have to add a socket if you want to use a Diamond."));
											}
											else
												ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No more gems can be added to this item."));
										}
									}
									else
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("These items cannot be used together."));
									}
									break;
							}
						}
						break;
						//  END_OF_ACCESSORY_REFINE & END_OF_ADD_ATTRIBUTES & END_OF_CHANGE_ATTRIBUTES

					case USE_BAIT:
						{
							if (m_pkFishingEvent || m_pkPreFishingEvent)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the Bait whilst fishing."));
								return false;
							}

							LPITEM weapon = GetWear(WEAR_WEAPON);

							if (!weapon || weapon->GetType() != ITEM_ROD)
								return false;

							if (weapon->GetSocket(2))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are exchanging the current Bait for %s."), item->GetName());
							}
							else
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You attached %s to the hook as bait."), item->GetName());
							}

							weapon->SetSocket(2, item->GetVnum(), true, "SOCKET_FISHING");
							item->SetCount(item->GetCount() - 1);
						}
						break;

					case USE_MOVE:
					case USE_TREASURE_BOX:
					case USE_MONEYBAG:
						break;

					case USE_AFFECT :
						{
							bool addAffect = true;
							// MT2009_PLUS_FLOWER_V1 (use): a flower of the Flower Event (value0 570,
							// Owsap's AFFECT_FLOWER_EVENT) - its own chances and levels (playerbot_flower.h).
							{
								bool FlowerEventUseItem(LPCHARACTER ch, LPITEM item, bool& result);
								bool bFlowerResult = false;
								if (FlowerEventUseItem(this, item, bFlowerResult))
									return bFlowerResult;
							}
							BYTE bPoint = (BYTE)item->GetValue(1);
							if (item->GetVnum() == UNIQUE_ITEM_SKIP_ITEM_DROP_PENALTY)
							{
								auto aff = FindAffect(AFFECT_NO_ITEM_DEATH_PENALTY);
								int curVal = 0;
								if (aff)
								{
									curVal = aff->lApplyValue;
									if (curVal >= 5)
									{
										ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is stacked to its maximum potential."));
										return false;
									}
								}

								AddAffect(item->GetValue(0), item->GetValue(1) , item->GetValue(2) + curVal, 0, item->GetValue(3), 0, true, false, item->GetVnum());
								item->SetCount(item->GetCount() - 1);
								addAffect = false;
							}
							else if (FindAffect(item->GetValue(0), item->GetValue(1)))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is already activated."));
								return false;
							}

							if (bPoint != POINT_NONE && FindAffect(AFFECT_POTION_BOOST, bPoint))
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is already activated."));
								return false;
							}

							if (addAffect)
							{
								AddAffect(item->GetValue(0), item->GetValue(1), item->GetValue(2), 0, item->GetValue(3), 0, false, false, item->GetVnum());
								item->SetCount(item->GetCount() - 1);
							}
						}
						break;

					case USE_CREATE_STONE:
						AutoGiveItem(number(28000, 28013));
						item->SetCount(item->GetCount() - 1);
						break;

					case USE_AFFECT_TARGET:
						{
							LPCHARACTER target = GetTarget();
							if (!target)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_AFFECT_TARGET_NO_TARGET"));
								return false;
							}

							if (target->IsPC())
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("USE_AFFECT_TARGET_PLAYER_CHARACTER"));
								return false;
							}

							if (item->GetVnum() == 72734 && target->GetRaceNum() != 2191)
							{
								ChatPacket(CHAT_TYPE_INFO, LC_TEXT("POISON_ONLY_GIANT_TURTLE"));
								return false;
							}

							const DWORD dwAffect = (DWORD)item->GetValue(0);
							const BYTE bPoint = (BYTE)item->GetValue(1);
							const long lValue = (long)item->GetValue(2);
							const long lDuration = (long)item->GetValue(3);

							target->AddAffect(dwAffect, bPoint, lValue, 0, lDuration, 0, true);
							item->SetCount(item->GetCount() - 1);
						}
						break;
						
					default:
						bool ret = quest::CQuestManager::instance().UseTypeItem(GetPlayerID(), item, false);
						if (!ret)
							sys_log(0, "UseItemEx ITEM_USE: Unknown type %s %d %d", item->GetName(), item->GetType(), item->GetSubType());
						return ret;
				}
			}
			break;

		case ITEM_METIN:
			{
				LPITEM item2;

				if (!IsValidItemPosition(DestCell) || !(item2 = GetItem(DestCell)))
					return false;

				if (item2->IsExchanging() || item2->IsEquipped()) // @fixme114
					return false;

				if (item2->GetType() == ITEM_PICK) return false;
				if (item2->GetType() == ITEM_ROD) return false;
				if (item2->GetType() == ITEM_HERB_KNIFE) return false;

				int i;

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				{
					DWORD dwVnum;

					// MT2009_PLUS_SOULSTONE9_V1 (cracked): a cracked stone stays in its socket but is
					// no kind - it never blocks a stone of the kind 0 it shares.
					if ((dwVnum = item2->GetSocket(i)) <= 2 || dwVnum == ITEM_BROKEN_METIN_VNUM)
						continue;

					TItemTable * p = ITEM_MANAGER::instance().GetTable(dwVnum);

					if (!p)
						continue;

					if (item->GetValue(5) == p->alValues[5])
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot attach several stones of the same type."));
						return false;
					}
				}

				if (item2->GetType() == ITEM_ARMOR)
				{
					if (!IS_SET(item->GetWearFlag(), WEARABLE_BODY) || !IS_SET(item2->GetWearFlag(), WEARABLE_BODY))
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This Spirit Stone cannot be attached to this type of item."));
						return false;
					}
				}
				else if (item2->GetType() == ITEM_WEAPON)
				{
					if (!IS_SET(item->GetWearFlag(), WEARABLE_WEAPON))
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot attach this Spirit Stone to a weapon."));
						return false;
					}
				}
				else
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No slot free."));
					return false;
				}

				for (i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
					if (item2->GetSocket(i) >= 1 && item2->GetSocket(i) <= 2 && item2->GetSocket(i) >= item->GetValue(2))
					{
#ifdef ENABLE_ADDSTONE_FAILURE
						if (number(1, 100) <= 30)
#else
						if (1)
#endif
						{
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have attached the Spirit Stone successfully."));
							item2->SetSocket(i, item->GetVnum(), true, "SOCKET_METIN");
							LogManager::instance().ItemLog(this, item2, "SOCKET_SUCCESS", item->GetName());
						}
						else
						{
							ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The Spirit Stone broke while being attached."));
							item2->SetSocket(i, ITEM_BROKEN_METIN_VNUM, true, "SOCKET_METIN");
							LogManager::instance().ItemLog(this, item2, "SOCKET_FAIL", item->GetName());
						}

						// MT2009_PLUS_DIGI_STACK_V1 (socket one stone): one stone of a stack (Digi Rasta).
						item->SetCount(item->GetCount() - 1);
						break;
					}

				if (i == ITEM_SOCKET_MAX_NUM)
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("No slot free."));
			}
			break;

		case ITEM_AUTOUSE:
		case ITEM_MATERIAL:
		case ITEM_SPECIAL:
		case ITEM_TOOL:
		case ITEM_LOTTERY:
			break;

		case ITEM_TOTEM:
			{
				if (!item->IsEquipped())
					EquipItem(item);
			}
			break;

		case ITEM_BLEND:

			sys_log(0,"ITEM_BLEND!!");
			if (Blend_Item_find(item->GetVnum()))
			{
				int		affect_type		= AFFECT_BLEND;
				int		apply_type		= item->GetSocket(0);
				int		apply_value		= item->GetSocket(1);
				int		apply_duration	= item->GetSocket(2);

				if (FindAffect(affect_type, apply_type))
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is already activated."));
				}
				else
				{
					if (FindAffect(AFFECT_EXP_BONUS_EURO_FREE, POINT_RESIST_MAGIC))
					{
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This effect is already activated."));
					}
					else
					{
						AddAffect(affect_type, apply_type, apply_value, 0, apply_duration, 0, false);
						item->SetCount(item->GetCount() - 1);
					}
				}
			}
			break;
		case ITEM_EXTRACT:
			{
				LPITEM pDestItem = GetItem(DestCell);
				if (NULL == pDestItem)
				{
					return false;
				}
				switch (item->GetSubType())
				{
				case EXTRACT_DRAGON_SOUL:
					if (pDestItem->IsDragonSoul())
					{
						return DSManager::instance().PullOut(this, NPOS, pDestItem, item);
					}
					return false;
				case EXTRACT_DRAGON_HEART:
					if (pDestItem->IsDragonSoul())
					{
						return DSManager::instance().ExtractDragonHeart(this, pDestItem, item);
					}
					return false;
				default:
					return false;
				}
			}
			break;

		case ITEM_NONE:
			sys_err("Item type NONE %s", item->GetName());
			break;

		default:
			bool ret = quest::CQuestManager::instance().UseTypeItem(GetPlayerID(), item, false);
			if (!ret)
				sys_log(0, "UseItemEx: Unknown type %s %d", item->GetName(), item->GetType());
			return ret;
	}

	return true;
}

int g_nPortalLimitTime = 10;

bool CHARACTER::UseItem(TItemPos Cell, TItemPos DestCell)
{
	WORD wCell = Cell.cell;
	BYTE window_type = Cell.window_type;
	//WORD wDestCell = DestCell.cell;
	//BYTE bDestInven = DestCell.window_type;
	LPITEM item;

	if (!CanHandleItem())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

	if (IsHorseInventory(item->GetItemPos()) && item->GetCell() >= GetInventoryMaxCount() ||
		IsHorseInventory(DestCell) && DestCell.cell >= GetInventoryMaxCount())
	{
		// summon horse item
		if (item->GetVnum() < 50051 && item->GetVnum() > 50053)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Summon your horse to get access to horse inventory."));
			return false;
		}
	}

	sys_log(0, "%s: USE_ITEM %s (inven %d, cell: %d)", GetName(), item->GetName(), window_type, wCell);

	if (item->IsExchanging())
		return false;

	if (!item->CanUsedBy(this))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item because you do not fulfil all the requirements."));
		return false;
	}

	if (IsStun() || IsBusyAction())
		return false;

	if (false == FN_check_item_sex(this, item))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are not able to use that item because you do not have the right gender."));
		return false;
	}

	// MT2009_PLUS_VEKIRION_V1 (chest rate): a box opened by a plain use
	// (ITEM_GIFTBOX - the chests, the Cor Draconis, the ItemShop boxes, all
	// through DropSpecialItemGroup) counts on its own key, up to 60 in
	// 500 ms: the client's quick opening (Ctrl + right click on a stack,
	// uiinventory.py) and the chest preview's buttons open them as fast as
	// the bag takes them. Everything else - potions, scrolls, other items -
	// keeps the engine's 5 uses in 500 ms on ePulse::ItemUse, and a box uses
	// none of that budget. Each box still asks for its 3 free slots (and a
	// Cor Draconis for the Alchemy quest) one by one, in UseItemEx.
	if (item->GetType() == ITEM_GIFTBOX)
	{
		if (!PulseManager::Instance().IncreaseCount(GetPlayerID(), MT2009_PLUS_PULSE_CHEST_OPEN, std::chrono::milliseconds(500), MT2009_PLUS_CHEST_OPEN_PER_500MS))
			return false;
	}
	else if (!PulseManager::Instance().IncreaseCount(GetPlayerID(), ePulse::ItemUse, std::chrono::milliseconds(500), 5))
	{
		return false;
	}

	//PREVENT_TRADE_WINDOW
	if (IS_SUMMON_ITEM(item->GetVnum()))
	{
		if (false == IS_SUMMONABLE_ZONE(GetMapIndex()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This function is not available right now."));
			return false;
		}

		if (CThreeWayWar::instance().IsThreeWayWarMapIndex(GetMapIndex()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use a Scroll of the Location whilst taking part in a kingdom battle."));
			return false;
		}
		int iPulse = thecore_pulse();

		if (iPulse - GetSafeboxLoadTime() < PASSES_PER_SEC(g_nPortalLimitTime))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("After opening the Storeroom you cannot use a Scroll of the Location for %d seconds."), g_nPortalLimitTime);

			if (test_server)
				ChatPacket(CHAT_TYPE_INFO, "[TestOnly]Pulse %d LoadTime %d PASS %d", iPulse, GetSafeboxLoadTime(), PASSES_PER_SEC(g_nPortalLimitTime));
			return false;
		}

		if (IsBusy())
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use a Scroll of the Location while another window is open."));
			return false;
		}

		if (IsAffectFlag(AFF_QUEST_BOX))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("CANNOT_TELEPORT_WHILE_CARRY_ITEM"));
			return false;
		}

		//PREVENT_REFINE_HACK

		{
			if (iPulse - GetRefineTime() < PASSES_PER_SEC(g_nPortalLimitTime))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("After a trade, you cannot use a scroll for another %d seconds."), g_nPortalLimitTime);
				return false;
			}
		}
		//END_PREVENT_REFINE_HACK

		//PREVENT_ITEM_COPY
		{
			if (iPulse - GetMyShopTime() < PASSES_PER_SEC(g_nPortalLimitTime))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("After opening a storeroom you cannot use a Scroll of the Location for another %d seconds."), g_nPortalLimitTime);
				return false;
			}

		}
		//END_PREVENT_ITEM_COPY

		if (item->GetVnum() != ITEM_MARRIAGE_RING)
		{
			PIXEL_POSITION posWarp;

			int x = 0;
			int y = 0;

			double nDist = 0;
			const double nDistant = 5000.0;

			if (item->GetVnum() == 22010 || item->GetVnum() == 22011)
			{
				x = item->GetSocket(0) - GetX();
				y = item->GetSocket(1) - GetY();
			}

			else if (item->GetVnum() == 22000 || item->GetVnum() == 22001)
			{
				SECTREE_MANAGER::instance().GetRecallPositionByEmpire(GetMapIndex(), GetEmpire(), posWarp);

				if (item->GetSocket(0) == 0)
				{
					x = posWarp.x - GetX();
					y = posWarp.y - GetY();
				}
				else
				{
					x = item->GetSocket(0) - GetX();
					y = item->GetSocket(1) - GetY();
				}
			}

			nDist = sqrt(pow((float)x,2) + pow((float)y,2));

			if (nDistant > nDist)
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use the Scroll of the Location because the distance is too small."));
				if (test_server)
					ChatPacket(CHAT_TYPE_INFO, "PossibleDistant %f nNowDist %f", nDistant,nDist);
				return false;
			}
		}

		//PREVENT_PORTAL_AFTER_EXCHANGE

		if (iPulse - GetExchangeTime()  < PASSES_PER_SEC(g_nPortalLimitTime))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("After a trade you cannot use a Scroll of the Location for %d seconds."), g_nPortalLimitTime);
			return false;
		}
		//END_PREVENT_PORTAL_AFTER_EXCHANGE

	}

	if ((item->GetVnum() == 50200) || (item->GetVnum() == 71049))
	{
		if (IsBusy())
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot open the Storeroom if another window is already open."));
			return false;
		}

	}
	//END_PREVENT_TRADE_WINDOW

	// @fixme150 BEGIN
	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item if you're using quests"));
		return false;
	}
	// @fixme150 END

	if (IS_SET(item->GetFlag(), ITEM_FLAG_LOG))
	{
		DWORD vid = item->GetVID();
		ITEM_COUNT oldCount = item->GetCount();
		DWORD vnum = item->GetVnum();

		char hint[ITEM_NAME_MAX_LEN + 32 + 1];
		int len = snprintf(hint, sizeof(hint) - 32, "%s", item->GetName());

		if (len < 0 || len >= (int) sizeof(hint) - 32)
			len = (sizeof(hint) - 32) - 1;

		bool ret = UseItemEx(item, DestCell);

		if (NULL == ITEM_MANAGER::instance().FindByVID(vid))
		{
			LogManager::instance().ItemLog(this, vid, vnum, "REMOVE", hint);
		}
		else if (oldCount != item->GetCount())
		{
			snprintf(hint + len, sizeof(hint) - len, " %u", oldCount - 1);
			LogManager::instance().ItemLog(this, vid, vnum, "USE_ITEM", hint);
		}
		// MT2009_PLUS_BATTLE_PASS_V1 (use): an item used, for the Battle Pass.
		if (ret)
		{
			void BattlePassOnUse(LPCHARACTER ch, DWORD vnum);
			BattlePassOnUse(this, vnum);
			// MT2009_PLUS_GOBLIN_V1 (used): a chest opened may give a Treasure Ticket.
			void GoblinOnUse(LPCHARACTER ch, DWORD vnum);
			GoblinOnUse(this, vnum);
		}
		return (ret);
	}
	else
	{
		const DWORD usedVnum = item->GetVnum();
		const bool usedOk = UseItemEx(item, DestCell);
		if (usedOk)
		{
			void BattlePassOnUse(LPCHARACTER ch, DWORD vnum);
			BattlePassOnUse(this, usedVnum);
			// MT2009_PLUS_GOBLIN_V1 (used ex): a chest opened may give a Treasure Ticket.
			void GoblinOnUse(LPCHARACTER ch, DWORD vnum);
			GoblinOnUse(this, usedVnum);
		}
		return usedOk;
	}
}

// ---- Garbage Bin (custom-patches/garbage_bin.patch) ------------------------
// A chat-command-only item destroy queue, protocol v1. No new packet header,
// no client EXE change: the client already sends plain chat commands
// (net.SendChatPacket) and reads replies back through CHAT_TYPE_COMMAND --
// the same channel RefineSuceeded/OpenPrivateShop/StoneDetect already use
// above -- so a stock-looking client from this line's own build is enough.
//
// prepare never touches the item. It only proves "this exact item, at this
// exact position, with these exact properties, belongs to the caller right
// now" and hands back a 32-hex-char token bound to that snapshot -- item
// unique ID (not VID, which a relog or core restart can reassign), position,
// vnum, count, sockets and attributes. commit re-checks every one of those
// from scratch against the live item before doing anything, and only then
// destroys through the engine's own standard path: RemoveFromCharacter()
// followed by M2_DESTROY_ITEM(), the same two calls playerbot_town.h already
// uses to make a bot's item vanish for good. That path is what sends
// HEADER_GD_ITEM_DESTROY to the db core (see ITEM_MANAGER::DestroyItem) --
// this line's db core already deletes the row on that header, so ownership,
// logging and DB persistence all apply exactly as they do for any other
// destroyed item. Nothing here duplicates or bypasses that.
//
// Both prepare and commit run the identical gauntlet DropItem() below already
// runs for "can this item be touched right now": CanHandleItem() for the
// character-wide busy gate (exchange, private/offline shop, safebox, cube,
// refine window -- all folded into IsBusy() already), item->IsExchanging(),
// item->isLocked(), the running-quest check, and ITEM_ANTIFLAG_DELETE for
// destroy-protected items. Equipment, the belt and the dragon soul inventory
// are excluded by construction: TItemPos::IsDefaultInventoryPosition() is
// true only for INVENTORY window_type at a cell below INVENTORY_MAX_NUM.
bool CHARACTER::GarbageBinCheckRateLimit()
{
	if (!HasPlayerData())
		return true;

	DWORD now = get_dword_time();
	if (playerData->m_dwLastGarbageBinCmdTime != 0 &&
		now - playerData->m_dwLastGarbageBinCmdTime < GARBAGE_BIN_MIN_INTERVAL_MS)
		return true;

	playerData->m_dwLastGarbageBinCmdTime = now;
	return false;
}

bool CHARACTER::MobDropCheckRateLimit()
{
	if (!HasPlayerData())
		return true;

	DWORD now = get_dword_time();
	if (playerData->m_dwLastMobDropCmdTime != 0 &&
		now - playerData->m_dwLastMobDropCmdTime < MOB_DROP_MIN_INTERVAL_MS)
		return true;

	playerData->m_dwLastMobDropCmdTime = now;
	return false;
}

const char * CHARACTER::GarbageBinPrepare(TItemPos Cell, DWORD dwVnum, ITEM_COUNT count, char szTokenOut[33])
{
	szTokenOut[0] = '\0';

	if (!HasPlayerData() || IsObserverMode() || IsDead())
		return "FORBIDDEN";

	if (!CanHandleItem())
		return "BUSY";

	if (!Cell.IsDefaultInventoryPosition() || Cell.cell >= GetInventoryMaxCount())
		return "FORBIDDEN";

	LPITEM item = GetItem(Cell);
	if (!item)
		return "NOT_FOUND";

	if (item->GetVnum() != dwVnum || item->GetCount() != count)
		return "CHANGED";

	if (item->IsExchanging() || item->isLocked())
		return "BUSY";

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning())
		return "BUSY";

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DELETE))
		return "FORBIDDEN";

	DWORD now = get_dword_time();
	auto & tokens = playerData->m_GarbageBinTokens;

	// Lazy expiry sweep: cheap, and every prepare/commit call is a natural
	// point to do it, so a separate timer buys nothing here.
	for (size_t i = 0; i < tokens.size(); )
	{
		if (tokens[i].dwExpireTime <= now)
			tokens.erase(tokens.begin() + i);
		else
			++i;
	}

	// A second prepare for the same item replaces its old token instead of
	// stacking a duplicate -- the client is expected to do this when a
	// prepare times out and it retries.
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		if (tokens[i].dwItemID == item->GetID())
		{
			tokens.erase(tokens.begin() + i);
			break;
		}
	}

	if (tokens.size() >= GARBAGE_BIN_MAX_TOKENS)
		return "FORBIDDEN";

	TGarbageBinToken tok;
	CryptoPP::AutoSeededRandomPool rng;
	BYTE raw[16];
	rng.GenerateBlock(raw, sizeof(raw));
	for (int i = 0; i < 16; ++i)
		snprintf(tok.token + i * 2, 3, "%02x", raw[i]);
	tok.token[32] = '\0';

	tok.dwItemID = item->GetID();
	tok.pos = Cell;
	tok.dwVnum = item->GetVnum();
	tok.count = item->GetCount();
	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		tok.alSockets[i] = item->GetSocket(i);
	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		tok.aAttr[i] = item->GetAttribute(i);
	tok.dwExpireTime = now + GARBAGE_BIN_TOKEN_TTL_MS;

	tokens.push_back(tok);
	strlcpy(szTokenOut, tok.token, 33);
	return NULL;
}

const char * CHARACTER::GarbageBinCommit(const char * szToken)
{
	if (!HasPlayerData() || IsObserverMode() || IsDead())
		return "FORBIDDEN";

	if (!szToken)
		return "INVALID";

	size_t len = strlen(szToken);
	if (len != 32)
		return "INVALID";

	for (size_t i = 0; i < len; ++i)
	{
		char c = szToken[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return "INVALID";
	}

	auto & tokens = playerData->m_GarbageBinTokens;

	size_t foundIdx = tokens.size();
	for (size_t i = 0; i < tokens.size(); ++i)
	{
		if (!strcmp(tokens[i].token, szToken))
		{
			foundIdx = i;
			break;
		}
	}

	if (foundIdx == tokens.size())
		return "NOT_FOUND";

	// Consumed here, before any validation result is even known, so a
	// retried or duplicated commit can never see this token again -- not to
	// destroy twice, and not to destroy whatever item later took this exact
	// slot.
	TGarbageBinToken tok = tokens[foundIdx];
	tokens.erase(tokens.begin() + foundIdx);

	DWORD now = get_dword_time();
	if (tok.dwExpireTime <= now)
		return "EXPIRED";

	if (!CanHandleItem())
		return "BUSY";

	if (!IsValidItemPosition(tok.pos))
		return "INVALID";

	LPITEM item = GetItem(tok.pos);
	if (!item || item->GetID() != tok.dwItemID)
		return "CHANGED";

	if (item->GetVnum() != tok.dwVnum || item->GetCount() != tok.count)
		return "CHANGED";

	for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
		if (item->GetSocket(i) != tok.alSockets[i])
			return "CHANGED";

	for (int i = 0; i < ITEM_ATTRIBUTE_MAX_NUM; ++i)
		if (!(item->GetAttribute(i) == tok.aAttr[i]))
			return "CHANGED";

	if (item->IsExchanging() || item->isLocked())
		return "BUSY";

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning())
		return "BUSY";

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DELETE))
		return "FORBIDDEN";

	LogManager::instance().ItemLog(this, item, "GARBAGE_BIN", "GARBAGE_BIN", true);

	SyncQuickslot(QUICKSLOT_TYPE_ITEM, tok.pos.cell, 255);

	M2_DESTROY_ITEM(item->RemoveFromCharacter());

	return NULL;
}

bool CHARACTER::DropItem(TItemPos Cell, ITEM_COUNT bCount)
{
	LPITEM item = NULL;

	if (!CanHandleItem())
	{
		if (NULL != DragonSoul_RefineWindow_GetOpener())
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot move the item within the refinement window."));
		return false;
	}
#ifdef ENABLE_NEWSTUFF
	if (!PulseManager::Instance().IncreaseClock(GetPlayerID(), ePulse::ItemDrop, std::chrono::milliseconds(1000)))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can only drop item once in every %d seconds."), 1);
		return false;
	}
#endif
	if (IsDead())
		return false;

	if (!IsValidItemPosition(Cell) || !(item = GetItem(Cell)))
		return false;

	// Same unguarded-window bug as CanEquipNow() -- see the comment there.
	if (IsHorseInventory(item->GetItemPos()) && item->GetCell() >= GetInventoryMaxCount())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Summon your horse to get access to horse inventory."));
		return false;
	}

	if (item->IsExchanging())
		return false;

	if (true == item->isLocked())
		return false;

	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		return false;

	if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP | ITEM_ANTIFLAG_GIVE))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot drop this item."));
		return false;
	}

	if (bCount == 0 || bCount > item->GetCount())
		bCount = item->GetCount();

	SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, 255);

	LPITEM pkItemToDrop;

	if (bCount == item->GetCount())
	{
		item->RemoveFromCharacter();
		pkItemToDrop = item;
	}
	else
	{
		if (bCount == 0)
		{
			if (test_server)
				sys_log(0, "[DROP_ITEM] drop item count == 0");
			return false;
		}

		item->SetCount(item->GetCount() - bCount);
		ITEM_MANAGER::instance().FlushDelayedSave(item);

		pkItemToDrop = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), bCount);

		// copy item socket -- by mhh
		FN_copy_item_socket(pkItemToDrop, item);

		char szBuf[51 + 1];
		snprintf(szBuf, sizeof(szBuf), "%u %u", pkItemToDrop->GetID(), pkItemToDrop->GetCount());
		LogManager::instance().ItemLog(this, item, "ITEM_SPLIT", szBuf);
	}

	PIXEL_POSITION pxPos = GetXYZ();

	if (pkItemToDrop->AddToGround(GetMapIndex(), pxPos))
	{
		auto time_to_destroy = g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPITEM];
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The dropped item will vanish in %d minutes."), time_to_destroy/60);
#ifdef ENABLE_NEWSTUFF
		pkItemToDrop->StartDestroyEvent(time_to_destroy);
#else
		pkItemToDrop->StartDestroyEvent();
#endif

		ITEM_MANAGER::instance().FlushDelayedSave(pkItemToDrop);

		char szHint[32 + 1];
		snprintf(szHint, sizeof(szHint), "%s %u %u", pkItemToDrop->GetName(), pkItemToDrop->GetCount(), pkItemToDrop->GetOriginalVnum());
		LogManager::instance().ItemLog(this, pkItemToDrop, "DROP", szHint);
		//Motion(MOTION_PICKUP);
	}

	return true;
}

bool CHARACTER::DropGold(YANG gold)
{
#ifdef DISABLE_GOLD_PLAYER_DROP
	return false;
#endif

	if (gold <= 0 || gold > GetGold())
		return false;

	if (!CanHandleItem())
		return false;

#ifdef ENABLE_NEWSTUFF
	if (g_GoldDropTimeLimitValue && !PulseManager::Instance().IncreaseClock(GetPlayerID(), ePulse::BoxOpening, std::chrono::milliseconds(g_GoldDropTimeLimitValue)))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can only drop gold once in every 10 seconds."));
		return false;
	}
#endif

	LPITEM item = ITEM_MANAGER::instance().CreateItem(1, gold);

	if (item)
	{
		PIXEL_POSITION pos = GetXYZ();

		if (item->AddToGround(GetMapIndex(), pos))
		{
			//Motion(MOTION_PICKUP);
			ChangeGold(-gold);

			if (gold > 1000)
				LogManager::instance().CharLog(this, gold, "DROP_GOLD", "");

#ifdef ENABLE_NEWSTUFF
			item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD]);
#else
			item->StartDestroyEvent();
#endif
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The item will disappear in %d minutes."), 150/60);
		}

		Save();
		return true;
	}

	return false;
}

#ifdef ENABLE_CHEQUE_SYSTEM
bool CHARACTER::DropCheque(int cheque)
{
#ifdef DISABLE_CHEQUE_DROP
	return false;
#else
	if (cheque <= 0 || cheque > GetCheque())
		return false;

	if (cheque >= CHEQUE_MAX)
		return false;

	if (!CanHandleItem())
		return false;

#ifdef ENABLE_NEWSTUFF
	if (g_GoldDropTimeLimitValue && !PulseManager::Instance().IncreaseClock(GetPlayerID(), ePulse::BoxOpening, std::chrono::milliseconds(g_GoldDropTimeLimitValue)))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You can only drop gold once in every 10 seconds."));
		return false;
	}
#endif

	LPITEM item = ITEM_MANAGER::instance().CreateItem(CHEQUE_VNUM, cheque);
	if (item)
	{
		PIXEL_POSITION pos = GetXYZ();
		if (item->AddToGround(GetMapIndex(), pos))
		{
			PointChange(POINT_CHEQUE, -cheque, true);
			LogManager::instance().CharLog(this, cheque, "DROP_CHEQUE", "");
			#ifdef ENABLE_NEWSTUFF
			item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_DROPGOLD]);
			#else
			item->StartDestroyEvent();
			#endif
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The item will disappear in %d minutes."), 150/60);
		}
		Save();
		return true;
	}
	return false;
#endif
}
#endif

bool CHARACTER::MoveItem(TItemPos Cell, TItemPos DestCell, ITEM_COUNT count)
{
	// Temporary trace, operator's request 17 September 2026: MoveItem's own
	// preamble was unguarded before this -- [DS_EQUIP_MOVE] alone showed zero
	// hits, meaning the real move never even reached that branch. Gate this
	// one on Cell (source) since item isn't resolved yet at function entry.
	const bool bDSMoveTraceSrc = (Cell.window_type == DRAGON_SOUL_INVENTORY) || DestCell.IsDragonSoulEquipPosition();
	if (bDSMoveTraceSrc)
	{
		sys_err("[DS_EQUIP_MOVE_TRACE] enter src_window=%u src_cell=%u dst_window=%u dst_cell=%u count=%u",
			Cell.window_type, Cell.cell, DestCell.window_type, DestCell.cell, (unsigned)count);
	}

	if (Cell.IsSamePosition(DestCell)) // @fixme196 (check same slot n same window aliases)
	{
		if (bDSMoveTraceSrc) sys_err("[DS_EQUIP_MOVE_TRACE] return false at IsSamePosition()");
		return false;
	}

	if (!IsValidItemPosition(Cell))
	{
		if (bDSMoveTraceSrc) sys_err("[DS_EQUIP_MOVE_TRACE] return false at !IsValidItemPosition(Cell)");
		return false;
	}

	LPITEM item = NULL;
	if (!(item = GetItem(Cell)))
	{
		if (bDSMoveTraceSrc) sys_err("[DS_EQUIP_MOVE_TRACE] return false at !GetItem(Cell) (no item at source cell)");
		return false;
	}

	const bool bDSMoveTrace = bDSMoveTraceSrc || item->IsDragonSoul() || item->GetType() == ITEM_SPECIAL_DS;
	const bool bForceCorStack = IsStackableCorDraconisVnum(item->GetVnum());
	if (bDSMoveTrace)
	{
		sys_err("[DS_EQUIP_MOVE_TRACE] item vnum=%u type=%u subtype=%u IsDragonSoul=%d qualified=%d refine_window_opener=%d",
			item->GetVnum(), item->GetType(), item->GetSubType(), item->IsDragonSoul() ? 1 : 0,
			DragonSoul_IsQualified() ? 1 : 0, (NULL != DragonSoul_RefineWindow_GetOpener()) ? 1 : 0);
	}

	if (item->IsExchanging())
	{
		if (bDSMoveTrace) sys_err("[DS_EQUIP_MOVE_TRACE] return false at IsExchanging()");
		return false;
	}

	if (item->GetCount() < count)
	{
		if (bDSMoveTrace) sys_err("[DS_EQUIP_MOVE_TRACE] return false at GetCount() < count (count=%u item_count=%u)", (unsigned)count, (unsigned)item->GetCount());
		return false;
	}

	if (INVENTORY == Cell.window_type && Cell.cell >= INVENTORY_MAX_NUM && IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
	{
		if (bDSMoveTrace) sys_err("[DS_EQUIP_MOVE_TRACE] return false at ITEM_FLAG_IRREMOVABLE horse-page check");
		return false;
	}

	if (true == item->isLocked())
	{
		if (bDSMoveTrace) sys_err("[DS_EQUIP_MOVE_TRACE] return false at isLocked()");
		return false;
	}

	if (!IsValidItemPosition(DestCell))
	{
		if (bDSMoveTrace) sys_err("[DS_EQUIP_MOVE_TRACE] return false at !IsValidItemPosition(DestCell)");
		return false;
	}

	if (IsHorseInventory(DestCell) && DestCell.cell >= GetInventoryMaxCount() ||
		IsHorseInventory(item->GetItemPos()) && Cell.cell >= GetInventoryMaxCount())
	{
		if (bDSMoveTrace) sys_err("[DS_EQUIP_MOVE_TRACE] return false at horse-inventory-cell check");
		return false;
	}

	// Metin2 SinglePlayer: an open safebox is no reason to refuse a move
	// inside the bag - splitting and pouring stacks beside it is the point
	// of having both windows open (blasty, 19 September). Every other busy
	// state still refuses.
	if (!CanHandleItem(false, false, BUSY_CAN_HANDLE_ITEM_EXCLUDE | BUSY_SAFEBOX))
	{
		if (bDSMoveTrace)
			sys_err("[DS_EQUIP_MOVE_TRACE] return false at !CanHandleItem() -- refine_window_opener=%d (this is the likely blocker: CanHandleItem() unconditionally refuses ALL moves while the DS refine window is open, confirmed same in the reference engine's own char_item.cpp)",
				(NULL != DragonSoul_RefineWindow_GetOpener()) ? 1 : 0);
		if (NULL != DragonSoul_RefineWindow_GetOpener())
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot move the item within the refinement window."));
		return false;
	}

	if (DestCell.IsBeltInventoryPosition() && false == CBeltInventoryHelper::CanMoveIntoBeltInventory(item))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot equip this item in your belt inventory."));
		if (bDSMoveTrace) sys_err("[DS_EQUIP_MOVE_TRACE] return false at CanMoveIntoBeltInventory()");
		return false;
	}

	if (Cell.IsEquipPosition())
	{
		if (!CanUnequipNow(item))
			return false;

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		int iWearCell = item->FindEquipCell(this);
		if (iWearCell == WEAR_WEAPON)
		{
			LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
			if (costumeWeapon && !UnequipItem(costumeWeapon))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot unequip the costume weapon. Not enough space."));
				return false;
			}

			if (!IsEmptyItemGrid(DestCell, item->GetSize(), Cell.cell))
				return UnequipItem(item);
		}
#endif
	}

	if (DestCell.IsEquipPosition())
	{
		if (item->IsDragonSoul())
		{
			sys_log(0, "[DS_EQUIP_MOVE] vnum=%u src_window=%u src_cell=%u dst_window=%u dst_cell=%u candidate=%d "
				"INVENTORY_MAX_NUM=%d WEAR_MAX_NUM=%d DRAGON_SOUL_EQUIP_SLOT_START=%d DRAGON_SOUL_EQUIP_SLOT_END=%d",
				item->GetVnum(), Cell.window_type, Cell.cell, DestCell.window_type, DestCell.cell,
				(int)DestCell.cell - (int)INVENTORY_MAX_NUM,
				(int)INVENTORY_MAX_NUM, (int)WEAR_MAX_NUM,
				(int)DRAGON_SOUL_EQUIP_SLOT_START, (int)DRAGON_SOUL_EQUIP_SLOT_END);
		}

		if (GetItem(DestCell))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have already equipped this kind of Dragon Stone."));

			return false;
		}

		EquipItem(item, DestCell.cell - INVENTORY_MAX_NUM);
	}
	else
	{
		if (item->IsDragonSoul())
		{
			if (item->IsEquipped())
			{
				return DSManager::instance().PullOut(this, DestCell, item);
			}
			else
			{
				if (DestCell.window_type != DRAGON_SOUL_INVENTORY)
				{
					return false;
				}

				if (!DSManager::instance().IsValidCellForThisItem(item, DestCell))
					return false;
			}
		}

		else if (DRAGON_SOUL_INVENTORY == DestCell.window_type)
			return false;

		LPITEM item2;

		if ((item2 = GetItem(DestCell)) && item != item2 && (item2->IsStackable() || bForceCorStack) &&
				(!IS_SET(item2->GetAntiFlag(), ITEM_ANTIFLAG_STACK) || bForceCorStack) &&
				item2->GetVnum() == item->GetVnum())
		{
			for (int i = 0; i < ITEM_SOCKET_MAX_NUM; ++i)
				if (item2->GetSocket(i) != item->GetSocket(i))
					return false;

			if (count == 0)
				count = item->GetCount();

			sys_log(0, "%s: ITEM_STACK %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell,
				DestCell.window_type, DestCell.cell, count);

			count = MIN(item2->GetMaxStack() - item2->GetCount(), count);

			item->SetCount(item->GetCount() - count);
			item2->SetCount(item2->GetCount() + count);
			return true;
		}

		if (!IsEmptyItemGrid(DestCell, item->GetSize(), Cell.cell))
			return false;

		if (count == 0 || count >= item->GetCount() ||
				(!item->IsStackable() && !bForceCorStack) ||
				(IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK) && !bForceCorStack))
		{
			sys_log(0, "%s: ITEM_MOVE %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell,
				DestCell.window_type, DestCell.cell, count);

			if (item->GetType() == ITEM_WEAPON && item->IsEquipped())
				OnWeaponChange(nullptr, item);

			item->RemoveFromCharacter();

#ifdef ENABLE_HIGHLIGHT_NEW_ITEM
			SetItem(DestCell, item, true);
#else
			SetItem(DestCell, item);
#endif
			if (INVENTORY == Cell.window_type && INVENTORY == DestCell.window_type)
				SyncQuickslot(QUICKSLOT_TYPE_ITEM, Cell.cell, DestCell.cell);
		}
		else if (count < item->GetCount())
		{
			sys_log(0, "%s: ITEM_SPLIT %s (window: %d, cell : %d) -> (window:%d, cell %d) count %d", GetName(), item->GetName(), Cell.window_type, Cell.cell,
				DestCell.window_type, DestCell.cell, count);

			item->SetCount(item->GetCount() - count);
			LPITEM item2 = ITEM_MANAGER::instance().CreateItem(item->GetVnum(), count);

			// copy socket -- by mhh
			FN_copy_item_socket(item2, item);

			item2->AddToCharacter(this, DestCell);

			char szBuf[51+1];
			snprintf(szBuf, sizeof(szBuf), "%u %d %d %d ", item2->GetID(), item2->GetCount(), item->GetCount(), item->GetCount() + item2->GetCount());
			LogManager::instance().ItemLog(this, item, "ITEM_SPLIT", szBuf);
		}
	}

	return true;
}

namespace NPartyPickupDistribute
{
	struct FFindOwnership
	{
		LPITEM item;
		LPCHARACTER owner;

		FFindOwnership(LPITEM item)
			: item(item), owner(NULL)
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (item->IsOwnership(ch))
				owner = ch;
		}
	};

	struct FCountNearMember
	{
		LPCHARACTER center;
		int		total;
		int		x, y;

		FCountNearMember(LPCHARACTER center )
			: total(0), x(center->GetX()), y(center->GetY()), center(center)
		{
		}

		void operator () (LPCHARACTER ch)
		{
			if (ch && center && ch->GetMapIndex() != center->GetMapIndex())
				return;

			if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				total += 1;
		}
	};

	struct FMoneyDistributor
	{
		int		total;
		LPCHARACTER	c;
		int		x, y;
		YANG	iMoney;

		FMoneyDistributor(LPCHARACTER center, YANG iMoney)
			: total(0), c(center), x(center->GetX()), y(center->GetY()), iMoney(iMoney)
		{
		}

		void operator ()(LPCHARACTER ch)
		{
			if (ch != c)
			{
				if (ch && c && ch->GetMapIndex() != c->GetMapIndex())
					return;

				if (DISTANCE_APPROX(ch->GetX() - x, ch->GetY() - y) <= PARTY_DEFAULT_RANGE)
				{
					ch->ChangeGold(iMoney);

					if (iMoney > 1000)
					{
						LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(ch, iMoney, "GET_GOLD", ""));
					}
				}
			}
		}
	};
}

void CHARACTER::GiveGold(YANG iAmount)
{
	if (iAmount <= 0)
		return;

	sys_log(0, "GIVE_GOLD: %s %lld", GetName(), iAmount);

	if (GetParty())
	{
		LPPARTY pParty = GetParty();

		YANG dwTotal = iAmount;
		YANG dwMyAmount = dwTotal;

		NPartyPickupDistribute::FCountNearMember funcCountNearMember(this);
		pParty->ForEachOnlineMember(funcCountNearMember);

		if (funcCountNearMember.total > 1)
		{
			YANG dwShare = dwTotal / funcCountNearMember.total;
			dwMyAmount -= dwShare * (funcCountNearMember.total - 1);

			NPartyPickupDistribute::FMoneyDistributor funcMoneyDist(this, dwShare);

			pParty->ForEachOnlineMember(funcMoneyDist);
		}

		ChangeGold(dwMyAmount);

		if (dwMyAmount > 1000)
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, dwMyAmount, "GET_GOLD", ""));
		}
	}
	else
	{
		ChangeGold(iAmount);

		if (iAmount > 1000)
		{
			LOG_LEVEL_CHECK(LOG_LEVEL_MAX, LogManager::instance().CharLog(this, iAmount, "GET_GOLD", ""));
		}
	}
}

#ifdef ENABLE_CHEQUE_SYSTEM
void CHARACTER::GiveCheque(int iAmount)
{
	if (iAmount <= 0)
		return;

	PointChange(POINT_CHEQUE, iAmount, true);
}
#endif

// MT2009_PLUS_DIGI_CLIENT_QOL_V1 (pickup sound): Digi Rasta's Pick-Up-Sound-Effect (Autor: Digi Rasta; nowy-system
// 0.19.0). While CHARACTER::PickupItem runs, every item it hands to the picker ends here, and the
// picker's client hears it: "PickupSound <vnum>" (1 = yang), a sound by the item's kind, at most
// one in 0.15 s (game.py, digiqol.py). Real players only - a bot's desc never gets the line.
static int s_iMt2009DigiPickupDepth = 0;

struct Mt2009DigiPickupScope
{
	Mt2009DigiPickupScope() { ++s_iMt2009DigiPickupDepth; }
	~Mt2009DigiPickupScope() { --s_iMt2009DigiPickupDepth; }
};

static void Mt2009DigiPickupSound(LPCHARACTER ch, DWORD dwVnum)
{
	if (!ch || !ch->IsPC() || !ch->GetDesc() || ch->GetDesc()->IsBot())
		return;
	ch->ChatPacket(CHAT_TYPE_COMMAND, "PickupSound %u", dwVnum);
}

void ChatPacketRecieveItem(LPCHARACTER ch, LPITEM item, ITEM_COUNT count)
{
	if (!ch || !item)
		return;

	if (s_iMt2009DigiPickupDepth > 0) // MT2009_PLUS_DIGI_CLIENT_QOL_V1 (pickup sound)
		Mt2009DigiPickupSound(ch, item->GetVnum());

	if (item->GetCell() >= INVENTORY_DEFAULT_MAX_NUM)
	{
		if (count < 2)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s received (Horse Inventory)"), item->GetName());
		else
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s %d received (Horse Inventory)"), item->GetName(), count);
	}
	else
	{
		if (count < 2)
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s received"), item->GetName());
		else
			ch->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s %d received"), item->GetName(), count);
	}

}

bool CHARACTER::PickupItem(DWORD dwVID)
{
	Mt2009DigiPickupScope digiPickupScope; // MT2009_PLUS_DIGI_CLIENT_QOL_V1 (pickup scope): the items handed over make a sound
	LPITEM item = ITEM_MANAGER::instance().FindByVID(dwVID);

	if (IsObserverMode())
		return false;

	if (!item || !item->GetSectree())
		return false;

	if (!item->DistanceValid(this))
		return false;

	// @fixme150 BEGIN
	if (item->GetType() == ITEM_QUEST)
	{
		if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot pickup this item if you're using quests"));
			return false;
		}
	}
	// @fixme150 END

	if (HasPlayerData())
	{
		if (playerData->m_lastPickupTime > get_dword_time())
		{
			return false;
		}
		playerData->m_lastPickupTime = get_dword_time() + (FindAffect(AFFECT_FAST_PICKUP) ? 100 : 500);
	}

	if (!CanHandleItem(false, false, 0))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot pickup item while busy."));
		return false;
	}

	// Cor Draconis (vnum 50255) must not be pickable by a playerbot -- a bot
	// could otherwise scoop up a Cor a real player dropped once ownership
	// protection expires. Real players are never blocked here. Operator's
	// explicit request, 17 September 2026. (The tilde pickup goes through
	// PickupItem for every item, so this covers it too.)
	//
	// MT2009_PLUS_COR_PARTY_PICKUP_V1: except a Cor that is a real player's of
	// the bot's own party - the companion collecting its owner's drop, a bot
	// invited into a player's party. The party branch below hands such an
	// item straight to its owner, so the bot never holds it ("Towarzysz nie
	// podnosi Corow", 27 September). Only an owned Cor: an ownerless one would
	// go to whichever member the party branch finds last, a bot as likely.
	if (item->GetVnum() == 50255 &&
		GetDesc() &&
		GetDesc()->IsBot())
	{
		LPCHARACTER corOwner = NULL;
		if (!item->IsOwnership(this) && GetParty() &&
			!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP))
		{
			NPartyPickupDistribute::FFindOwnership findOwner(item);
			GetParty()->ForEachOnlineMember(findOwner);
			corOwner = findOwner.owner;
		}
		if (!corOwner || corOwner == this || !corOwner->GetDesc() || corOwner->GetDesc()->IsBot())
		{
			sys_log(0, "[DS_COR_DROP] pickup blocked: bot pid=%u name=%s tried to pick up Cor Draconis id=%u",
				GetPlayerID(), GetName(), item->GetID());
			return false;
		}
		sys_log(0, "[DS_COR_DROP] party pickup: bot pid=%u name=%s picks up Cor Draconis id=%u for %s",
			GetPlayerID(), GetName(), item->GetID(), corOwner->GetName());
	}

	if (item->IsOwnership(this))
	{
		if (item->GetType() == ITEM_ELK)
		{
			AddPlayerStat(PLAYER_STATS_GOLD_FLAG, item->GetCount());
			GiveGold(item->GetCount());
			Mt2009DigiPickupSound(this, 1); // MT2009_PLUS_DIGI_CLIENT_QOL_V1 (pickup yang)
			item->RemoveFromGround();

			M2_DESTROY_ITEM(item);

			Save();
		}
#if defined(ENABLE_CHEQUE_SYSTEM) && !defined(DISABLE_CHEQUE_DROP)
		else if (item->GetVnum() == CHEQUE_VNUM)
		{
			if (item->GetCount() + GetCheque() > CHEQUE_MAX - 1)
				return false;
			GiveCheque(item->GetCount());
			item->RemoveFromGround();
			M2_DESTROY_ITEM(item);
			Save();
		}
#endif

		else
		{
			auto finalItem = AutoStackItem(item);
			if (finalItem)
			{
				ChatPacketRecieveItem(this, finalItem, 1);
				if (finalItem->GetType() == ITEM_QUEST)
					quest::CQuestManager::instance().PickupItem(GetPlayerID(), finalItem);
				return true;
			}

			int iEmptyCell = GetEmptyInventoryEx(item);
			if (iEmptyCell == -1)
			{
				sys_log(0, "No empty inventory pid %u size %ud itemid %u", GetPlayerID(), item->GetSize(), item->GetID());
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have too many items in your inventory."));
				return false;
			}

			item->RemoveFromGround();

			item->AddToCharacter(this, TItemPos(item->GetWindowInventoryEx(), iEmptyCell));

			char szHint[32+1];
			snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
			LogManager::instance().ItemLog(this, item, "GET", szHint, true);
			ChatPacketRecieveItem(this, item, 1);

			if (item->GetType() == ITEM_QUEST)
				quest::CQuestManager::instance().PickupItem(GetPlayerID(), item);
		}

		//Motion(MOTION_PICKUP);
		return true;
	}
	else if (!IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_GIVE | ITEM_ANTIFLAG_DROP) && GetParty())
	{
		NPartyPickupDistribute::FFindOwnership funcFindOwnership(item);

		GetParty()->ForEachOnlineMember(funcFindOwnership);

		LPCHARACTER owner = funcFindOwnership.owner;
		// @fixme115
		if (!owner)
			return false;

		// MT2009_PLUS_PICKUP_FILTER_V1 (party): a party member - the companion,
		// a bot of the party - does not pick up for a member what that member
		// has filtered out (Ctrl+Z, uipickupfilter.py): it stays on the ground.
		{
			bool Mt2009PlusPickupFilterAllows(LPCHARACTER ch, LPITEM item);
			if (owner != this && !Mt2009PlusPickupFilterAllows(owner, item))
				return false;
		}

		// A stackable the owner already carries joins that stack first, as the
		// owner's own pickup above does: straight to an empty cell, every potion
		// a party member picked up for somebody took a slot of its own. What a
		// full stack cannot take goes on to the empty cell below.
		auto finalItem = owner->AutoStackItem(item);
		if (finalItem)
		{
			if (owner == this)
				ChatPacketRecieveItem(this, finalItem, 1);
			else
			{
				owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s receives %s."), owner->GetName(), finalItem->GetName());
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Item Trade: %s, %s"), owner->GetName(), finalItem->GetName());
			}
			if (finalItem->GetType() == ITEM_QUEST)
				quest::CQuestManager::instance().PickupItem(owner->GetPlayerID(), finalItem);
			return true;
		}

		int iEmptyCell = -1;
		if (!(owner && (iEmptyCell = owner->GetEmptyInventoryEx(item)) != -1))
		{
			// mt2009 edit: party member cannot recieve an item if the owner has no inventory space
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Owner has too many items in his inventory."));
			return false;
		}

		item->RemoveFromGround();

		item->AddToCharacter(owner, TItemPos(item->GetWindowInventoryEx(), iEmptyCell));

		char szHint[32+1];
		snprintf(szHint, sizeof(szHint), "%s %u %u", item->GetName(), item->GetCount(), item->GetOriginalVnum());
		LogManager::instance().ItemLog(owner, item, "GET", szHint, true);

		if (owner == this)
		{
			ChatPacketRecieveItem(this, item, 1);
		}
		else
		{
			owner->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("%s receives %s."), owner->GetName(), item->GetName());
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Item Trade: %s, %s"), owner->GetName(), item->GetName());
		}

		if (item->GetType() == ITEM_QUEST)
			quest::CQuestManager::instance().PickupItem (owner->GetPlayerID(), item);

		return true;
	}

	return false;
}

// "Podnies caly drop" - the ` key (client-root/pickupnearby.py; the shape is
// SIZOWSKI's patch of 18 September). Every item on the ground within the
// pickup range that is this character's or may be its party's, nearest
// first, handed to PickupItem one at a time - so ownership, the party's
// split, stacking and every item's own rule are the ones the Z key applies,
// and PickupItem itself is not touched. It allows one pickup per half second
// (m_lastPickupTime); the batch lifts that for its own items and leaves it
// set afterwards, and is itself allowed once per half second a character.
// A pile stops the batch at PICKUP_NEARBY_MAX items, and a bag with no room
// for the next one stops it at once, rather than saying so for every item.
// MT2009_PLUS_PICKUP_FILTER_V1 (server-patches/playerqol): what the Z key and
// a loot pet pick up (the operator, 28 September: "filtrowanie zbieranego
// dropu, jak w auto lowach"). The client's window (uipickupfilter.py) sends
// "/pickup_filter <on> <kinds>" after every login and every change, with the
// auto-hunt's thirteen kinds (cmd_general.cpp, AutoHuntLootKind); a character
// with no entry takes everything, and yang is always taken.
std::map<DWORD, DWORD> g_mapMt2009PlusPickupFilter;

// MT2009_PLUS_PICKUP_BONUS_FILTER_V1 (server-patches/playerqol): "Bonus", the
// third choice of an equipment kind in the filter (the owner, 3 October:
// "podnos tylko z bonusem dla kategorii eq"). Such a kind's item is taken
// only with at least the asked number of its five bonuses
// (CItem::GetAttributeCount); fewer, and it stays on the ground for the Z
// key, a loot pet, Auto Lowy and the companion alike. Kept per character as
// (min << 16) | kinds; only the equipment kinds - weapons, armour, helmets,
// shields, bracelets, shoes, necklaces, earrings - can have it.
std::map<DWORD, DWORD> g_mapMt2009PlusPickupBonus;
static const DWORD MT2009_PLUS_PICKUP_BONUS_KINDS =
	(1 << 0) | (1 << 1) | (1 << 7) | (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11) | (1 << 12);

static DWORD Mt2009PlusPickupKind(LPITEM item)
{
	switch (item->GetType())
	{
		case ITEM_WEAPON:
			return item->GetSubType() == WEAPON_ARROW ? (1 << 6) : (1 << 0);
		case ITEM_ARMOR:
			switch (item->GetSubType())
			{
				case ARMOR_BODY: return 1 << 1;
				case ARMOR_HEAD: return 1 << 7;
				case ARMOR_SHIELD: return 1 << 8;
				case ARMOR_WRIST: return 1 << 9;
				case ARMOR_FOOTS: return 1 << 10;
				case ARMOR_NECK: return 1 << 11;
				case ARMOR_EAR: return 1 << 12;
				default: return 1 << 2;
			}
		case ITEM_RING:
		case ITEM_BELT:
			return 1 << 2;
		case ITEM_USE:
			switch (item->GetSubType())
			{
				case USE_POTION:
				case USE_POTION_NODELAY:
				case USE_ABILITY_UP:
					return 1 << 3;
				default:
					return 1 << 6;
			}
		case ITEM_SKILLBOOK:
		case ITEM_SKILLFORGET:
			return 1 << 4;
		case ITEM_METIN:
			return 1 << 5;
		default:
			return 1 << 6;
	}
}

bool Mt2009PlusPickupFilterAllows(LPCHARACTER ch, LPITEM item)
{
	if (!ch || !item || item->GetType() == ITEM_ELK)
		return true;
	std::map<DWORD, DWORD>::const_iterator it = g_mapMt2009PlusPickupFilter.find(ch->GetPlayerID());
	if (it == g_mapMt2009PlusPickupFilter.end())
		return true;
	const DWORD kind = Mt2009PlusPickupKind(item);
	if ((kind & it->second) == 0)
		return false;
	// MT2009_PLUS_PICKUP_BONUS_FILTER_V1 (allows): a "Bonus" kind's item with
	// fewer bonuses than asked is left.
	std::map<DWORD, DWORD>::const_iterator bonus = g_mapMt2009PlusPickupBonus.find(ch->GetPlayerID());
	if (bonus != g_mapMt2009PlusPickupBonus.end() && (kind & bonus->second & 0xFFFF) != 0)
		return item->GetAttributeCount() >= (int) (bonus->second >> 16);
	return true;
}

// "/pickup_filter <on> <kinds> <bonus kinds> <min bonuses>" (cmd_general.cpp):
// no equipment kind among them clears the character's entry. Returns the
// kinds kept and puts the number used into minCount.
DWORD Mt2009PlusSetPickupBonus(DWORD pid, DWORD kinds, int& minCount)
{
	minCount = MINMAX(1, minCount, (int) ITEM_ATTRIBUTE_NORM_NUM);
	kinds &= MT2009_PLUS_PICKUP_BONUS_KINDS;
	if (!kinds)
		g_mapMt2009PlusPickupBonus.erase(pid);
	else
		g_mapMt2009PlusPickupBonus[pid] = ((DWORD) minCount << 16) | kinds;
	return kinds;
}

int CHARACTER::PickupNearbyItems()
{
	const size_t PICKUP_NEARBY_MAX = 40;
	const DWORD PICKUP_NEARBY_COOLDOWN_MS = 500;
	if (!HasPlayerData() || IsObserverMode() || IsDead() || !GetSectree())
		return 0;
	static std::map<DWORD, DWORD> s_nextBatch;
	const DWORD now = get_dword_time();
	DWORD& next = s_nextBatch[GetPlayerID()];
	if (now < next)
		return 0;
	next = now + PICKUP_NEARBY_COOLDOWN_MS;
	if (!CanHandleItem(false, false, 0))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot pickup item while busy."));
		return 0;
	}

	struct FCollectNearby
	{
		LPCHARACTER ch;
		std::vector<std::pair<int, DWORD> > found;
		explicit FCollectNearby(LPCHARACTER c) : ch(c) {}
		void operator()(LPENTITY ent)
		{
			if (!ent || !ent->IsType(ENTITY_ITEM))
				return;
			LPITEM item = static_cast<LPITEM>(ent);
			if (!item->GetSectree() || !item->DistanceValid(ch))
				return;
			// Somebody else's drop is refused by PickupItem without a word; not
			// asking at all is cheaper. With a party, PickupItem decides.
			if (!item->IsOwnership(ch) && !ch->GetParty())
				return;
			// MT2009_PLUS_PICKUP_FILTER_V1 (collect): left on the ground.
			if (!Mt2009PlusPickupFilterAllows(ch, item))
				return;
			found.push_back(std::make_pair(
					DISTANCE_APPROX(ch->GetX() - item->GetX(), ch->GetY() - item->GetY()),
					(DWORD)item->GetVID()));
		}
	} collect(this);
	GetSectree()->ForEachAround(collect);
	std::sort(collect.found.begin(), collect.found.end());

	const DWORD held = playerData->m_lastPickupTime;
	int picked = 0;
	for (size_t i = 0; i < collect.found.size() && i < PICKUP_NEARBY_MAX; ++i)
	{
		// Re-found by VID: an earlier pickup can move or remove other items.
		LPITEM item = ITEM_MANAGER::instance().FindByVID(collect.found[i].second);
		if (!item || !item->GetSectree() || !item->DistanceValid(this))
			continue;
		const BYTE size = item->GetSize();
		playerData->m_lastPickupTime = 0;
		if (PickupItem(collect.found[i].second))
		{
			++picked;
			continue;
		}
		if (GetEmptyInventory(size) < 0)
			break;
	}
	playerData->m_lastPickupTime = std::max<DWORD>(held, now + 500);
	return picked;
}

bool CHARACTER::SwapItem(WORD bCell, WORD bDestCell)
{
	if (!CanHandleItem())
		return false;

	TItemPos srcCell(INVENTORY, bCell), destCell(INVENTORY, bDestCell);

	//if (bCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM || bDestCell >= INVENTORY_MAX_NUM + WEAR_MAX_NUM)
	if (srcCell.IsDragonSoulEquipPosition() || destCell.IsDragonSoulEquipPosition())
		return false;

	if (bCell == bDestCell)
		return false;

	if (srcCell.IsEquipPosition() && destCell.IsEquipPosition())
		return false;

	LPITEM item1, item2;

	if (srcCell.IsEquipPosition())
	{
		item1 = GetInventoryItem(bDestCell);
		item2 = GetInventoryItem(bCell);
	}
	else
	{
		item1 = GetInventoryItem(bCell);
		item2 = GetInventoryItem(bDestCell);
	}

	if (!item1 || !item2)
		return false;

	if (item1 == item2)
	{
	    sys_log(0, "[WARNING][WARNING][HACK USER!] : %s %d %d", m_stName.c_str(), bCell, bDestCell);
	    return false;
	}

	if (!IsEmptyItemGrid(TItemPos (INVENTORY, item1->GetCell()), item2->GetSize(), item1->GetCell()))
		return false;

	if (TItemPos(EQUIPMENT, item2->GetCell()).IsEquipPosition())
	{
		BYTE bEquipCell = item2->GetCell() - INVENTORY_MAX_NUM;
		WORD bInvenCell = item1->GetCell();

		if (false == CanUnequipNow(item2) || false == CanEquipNow(item1))
			return false;

		if (bEquipCell != item1->FindEquipCell(this))
			return false;

		item2->RemoveFromCharacter();

		if (item1->EquipTo(this, bEquipCell))
		{
			item2->AddToCharacter(this, TItemPos(INVENTORY, bInvenCell));
			if (item1->GetType() == ITEM_WEAPON)
				OnWeaponChange(item1, item2);


			// teoretycznie to robi ComputePoints, ale nie wiem czy item::EquipTo powinien wykonywac ta funkcje
			// dla bezpieczentwa tutaj chujowy kod
			if (GetHP() > GetMaxHP())
				PointChange(POINT_HP, GetMaxHP() - GetHP());

			if (GetSP() > GetMaxSP())
				PointChange(POINT_SP, GetMaxSP() - GetSP());
		}
		else
			sys_err("SwapItem cannot equip %s! item1 %s", item2->GetName(), item1->GetName());
	}
	else
	{
		WORD bCell1 = item1->GetCell();
		WORD bCell2 = item2->GetCell();

		item1->RemoveFromCharacter();
		item2->RemoveFromCharacter();

		item1->AddToCharacter(this, TItemPos(INVENTORY, bCell2));
		item2->AddToCharacter(this, TItemPos(INVENTORY, bCell1));
	}

	if (HasPlayerData()) {

		playerData->m_swapItemCount++;
		if (get_dword_time() - playerData->m_lastItemEquipTime > 700)
		{
			playerData->m_swapItemCount = 0;
		}

		ChatDebug("swapCount %d", playerData->m_swapItemCount);

		if (playerData->m_swapItemCount > 10 && GetDesc())
		{
			LogManager::instance().HackLog("FAST_ITEM_SWAP", this);
			GetDesc()->DelayedDisconnect(1);
		}
	}

	return true;
}

bool CHARACTER::UnequipItem(LPITEM item, LPITEM keyItem)
{
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	int iWearCell = item->FindEquipCell(this);
	if (iWearCell == WEAR_WEAPON)
	{
		LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
		if (costumeWeapon && !UnequipItem(costumeWeapon))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot unequip the costume weapon. Not enough space."));
			return false;
		}
	}
#endif

	if (false == CanUnequipNow(item, NPOS, NPOS, keyItem))
		return false;

	int pos = GetEmptyInventoryEx(item);

	// HARD CODING
	if (item->GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
		ShowAlignment(true);
	else if (item->GetVnum() == UNIQUE_ITEM_HIDE_LEVEL)
		UpdatePacket();
	else if (item->GetVnum() == UNIQUE_ITEM_HORSE_STAMINA)
		SetStopStaminaConsume(false);

	if (item->GetType() == ITEM_PICK)
	{
		mining_cancel();
	}

	item->RemoveFromCharacter();
	item->AddToCharacter(this, TItemPos(item->GetWindowInventoryEx(), pos));

	if (item->GetType() == ITEM_WEAPON)
		OnWeaponChange(nullptr, item);

	CheckMaximumPoints();

	return true;
}

bool CHARACTER::EquipItem(LPITEM item, int iCandidateCell)
{
	// Temporary trace, operator's request 17 September 2026 -- gated on the
	// literal type check (not IsDragonSoul(), which only covers ITEM_DS,
	// not ITEM_SPECIAL_DS) so both types are covered as asked.
	// MT2009_PLUS_DS_TRACE_PLAYERS_V1 (server-patches/dstraceplayers): the Dragon
	// Stone equip trace is for players; bots wear stones all day (playerbot_alchemy.h)
	// and eight syserr lines a stone buried everything else.
	const bool bDSTrace = (item->GetType() == ITEM_DS || item->GetType() == ITEM_SPECIAL_DS) &&
		!(GetDesc() && GetDesc()->IsBot());

	if (bDSTrace)
	{
		sys_err(
			"[DS_EQUIP_TRACE] start vnum=%u type=%u subtype=%u window=%u cell=%u "
			"candidate=%d equipable=%d exchanging=%d qualified=%d",
			item->GetVnum(),
			item->GetType(),
			item->GetSubType(),
			item->GetWindow(),
			item->GetCell(),
			iCandidateCell,
			item->IsEquipable(),
			item->IsExchanging(),
			DragonSoul_IsQualified()
		);
	}

	if (item->IsExchanging())
	{
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at IsExchanging()");
		return false;
	}

	if (false == item->IsEquipable())
	{
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at IsEquipable()");
		return false;
	}

	bool bCanEquipNowResult = CanEquipNow(item);
	if (bDSTrace) sys_err("[DS_EQUIP_TRACE] CanEquipNow(item)=%d", bCanEquipNowResult ? 1 : 0);

	if (false == bCanEquipNowResult)
	{
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at CanEquipNow()");
		return false;
	}

	int iWearCell = item->FindEquipCell(this, iCandidateCell);
	if (bDSTrace) sys_err("[DS_EQUIP_TRACE] FindEquipCell(this, candidate=%d)=%d iWearCell=%d", iCandidateCell, iWearCell, iWearCell);

	if (iWearCell < 0)
	{
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at iWearCell < 0");
		return false;
	}

	if (bDSTrace)
	{
		LPITEM wearItem = GetWear(iWearCell);
		LPITEM invSlotItem = GetInventoryItem(INVENTORY_MAX_NUM + iWearCell);
		sys_err("[DS_EQUIP_TRACE] GetWear(%d)=%s GetInventoryItem(INVENTORY_MAX_NUM+iWearCell=%d)=%s",
			iWearCell, wearItem ? wearItem->GetName() : "NULL",
			(int)INVENTORY_MAX_NUM + iWearCell,
			invSlotItem ? invSlotItem->GetName() : "NULL");
	}

	if (iWearCell == WEAR_BODY && IsRiding() && (item->GetVnum() >= 11901 && item->GetVnum() <= 11904))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot wear Tuxido on a horse."));
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at Tuxido-on-horse check");
		return false;
	}

	if (iWearCell != WEAR_ARROW && IsPolymorphed())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot change the equipped item while you are transformed."));
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at IsPolymorphed()");
		return false;
	}

	if (FN_check_item_sex(this, item) == false)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are not able to use that item because you do not have the right gender."));
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at FN_check_item_sex()");
		return false;
	}

	// @fixme314 BEGIN
	if (item && item->GetType() == ITEM_UNIQUE && item->GetCount() > 1)
	{
		#ifdef ENABLE_UNIQUE_ITEM_AUTOSPLIT
		if (!item->IsStackable())
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The unique item is not stackable."));
			return false;
		}
		else if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The unique item has the antiflag stack."));
			return false;
		}

		const int iEmptyPos = GetEmptyInventoryEx(item);
		if (iEmptyPos == -1)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You don't have enough space to auto-split the unique item."));
			return false;
		}

		const auto newCount = item->GetCount() - 1;
		MoveItem(item->GetItemPos(), TItemPos(item->GetWindowInventoryEx(), iEmptyPos), newCount);

		if (item->GetCount() != 1)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need to split the item to 1 unit to equip it."));
			return false;
		}
		#else
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You need to split the item to 1 unit to equip it."));
		return false;
		#endif
	}
	// @fixme314 END
	// (ITEM_UNIQUE-only block above -- unreachable for ITEM_DS/ITEM_SPECIAL_DS)

	if (item->IsRideItem())
	{
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		// A mount seal (IsNewMountItem()) does not fall under the old "you're
		// already riding, get off first" refusal below -- equipping one just
		// dismounts the traditional horse first, the same way ENABLE_MOUNT_
		// COSTUME_EX_SYSTEM's own (still-disabled) branch does for every ride
		// item. Old-style ride items (IsOldMountItem()) keep their existing
		// behavior untouched.
		if (item->IsNewMountItem())
		{
			if (GetHorse() || IsHorseRiding())
			{
				StopRiding();
				HorseSummon(false);
			}
		}
		else
#endif
		{
			#ifdef ENABLE_MOUNT_COSTUME_EX_SYSTEM
				#ifdef ENABLE_NEWSTUFF
				// block mount spawn
				if (g_NoMountAtGuildWar && GetWarMap())
				{
					if (IsRiding())
						StopRiding();
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You're already riding. Get off first."));
					return false;
				}
				#endif

				// unsummon horse first
				if (GetHorse() || IsHorseRiding()) {
					StopRiding();
					HorseSummon(false);
				}

				if (GetHorse() || IsHorseRiding())
					return false;
			#else
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You're already riding. Get off first."));
			return false;
			#endif
		}
	}

	DWORD dwCurTime = get_dword_time();

	// MT2009_PLUS_MOUNT_QUICKSWAP_V1 (stand still), server-patches/mountquickswap:
	// a mount seal is put on at once, also right after an attack or a skill,
	// as a horse is called and ridden at once - the 1.5 s wait is there
	// against swapping weapons and armour in a fight, and a seal only
	// calls the mount (Ctrl+G with the seal in the bag, a click on it).
	// The equip counter (CanEquipNow, 5 in 500 ms) and FAST_ITEM_SWAP stay.
	bool bStandStillCheck = iWearCell != WEAR_ARROW;
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (item->IsNewMountItem())
		bStandStillCheck = false;
#endif
	if (bStandStillCheck
		&& (dwCurTime - GetLastAttackTime() <= 1500 || dwCurTime - m_dwLastSkillTime <= 1500))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have to stand still to equip the item."));
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at stand-still timing check");
		return false;
	}

#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
	if (iWearCell == WEAR_WEAPON)
	{
		if (item->GetType() == ITEM_WEAPON)
		{
			LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
			if (costumeWeapon && costumeWeapon->GetValue(3) != item->GetSubType() && !UnequipItem(costumeWeapon))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot unequip the costume weapon. Not enough space."));
				return false;
			}
		}
		else //fishrod/pickaxe
		{
			LPITEM costumeWeapon = GetWear(WEAR_COSTUME_WEAPON);
			if (costumeWeapon && !UnequipItem(costumeWeapon))
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot unequip the costume weapon. Not enough space."));
				return false;
			}
		}
	}
	else if (iWearCell == WEAR_COSTUME_WEAPON)
	{
		if (item->GetType() == ITEM_COSTUME && item->GetSubType() == COSTUME_WEAPON)
		{
			LPITEM pkWeapon = GetWear(WEAR_WEAPON);
			if (!pkWeapon || pkWeapon->GetType() != ITEM_WEAPON || item->GetValue(3) != pkWeapon->GetSubType())
			{
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot equip the costume weapon. Wrong equipped weapon."));
				return false;
			}
		}
	}
#endif

	if (item->IsDragonSoul())
	{
		if (bDSTrace)
			sys_err("[DS_EQUIP_TRACE] DS branch: GetInventoryItem(INVENTORY_MAX_NUM+iWearCell=%d)=%s",
				(int)INVENTORY_MAX_NUM + iWearCell,
				GetInventoryItem(INVENTORY_MAX_NUM + iWearCell) ? GetInventoryItem(INVENTORY_MAX_NUM + iWearCell)->GetName() : "NULL");

		if(GetInventoryItem(INVENTORY_MAX_NUM + iWearCell))
		{
			ChatPacket(CHAT_TYPE_INFO, "You are already carrying a Dragon Stone of this type.");
			if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at 'already carrying this type' check");
			return false;
		}

		bool bEquipToResult = item->EquipTo(this, iWearCell);
		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] EquipTo(this, iWearCell=%d)=%d", iWearCell, bEquipToResult ? 1 : 0);

		if (!bEquipToResult)
		{
			if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at EquipTo()");
			return false;
		}

		if (bDSTrace) sys_err("[DS_EQUIP_TRACE] DS branch reached success path (iWearCell=%d)", iWearCell);
	}

	else
	{
		LPITEM equippedItem = GetWear(iWearCell);
		if (bDSTrace)
			sys_err("[DS_EQUIP_TRACE] non-DS branch (ITEM_SPECIAL_DS falls here -- IsDragonSoul() only covers ITEM_DS): equippedItem=%s",
				equippedItem ? equippedItem->GetName() : "NULL");

		if (equippedItem && !IS_SET(equippedItem->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		{
			if (item->GetWearFlag() == WEARABLE_ABILITY)
			{
				if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at WEARABLE_ABILITY-with-occupant check");
				return false;
			}

			if (false == SwapItem(item->GetCell(), INVENTORY_MAX_NUM + iWearCell))
			{
				if (bDSTrace) sys_err("[DS_EQUIP_TRACE] return false at SwapItem()");
				return false;
			}
		}
		else
		{
			BYTE bOldCell = item->GetCell();

			bool bEquipToResult2 = item->EquipTo(this, iWearCell);
			if (bDSTrace) sys_err("[DS_EQUIP_TRACE] non-DS branch EquipTo(this, iWearCell=%d)=%d", iWearCell, bEquipToResult2 ? 1 : 0);

			if (bEquipToResult2)
			{
				if (item->GetType() == ITEM_WEAPON)
					OnWeaponChange(item, equippedItem);

				SyncQuickslot(QUICKSLOT_TYPE_ITEM, bOldCell, iWearCell);
			}
		}
	}

	if (true == item->IsEquipped())
	{
		if (-1 != item->GetProto()->cLimitRealTimeFirstUseIndex)
		{
			if (0 == item->GetSocket(1))
			{
				long duration = (0 != item->GetSocket(0)) ? item->GetSocket(0) : item->GetProto()->aLimits[(unsigned char)(item->GetProto()->cLimitRealTimeFirstUseIndex)].lValue;

				if (0 == duration)
					duration = 60 * 60 * 24 * 7;

				item->SetSocket(0, time(0) + duration, true, "SOCKET_REALTIME");
				item->StartRealTimeExpireEvent();
			}

			item->SetSocket(1, item->GetSocket(1) + 1,true, "SOCKET_REALTIME");
		}

		if (item->GetVnum() == UNIQUE_ITEM_HIDE_ALIGNMENT_TITLE)
			ShowAlignment(false);
		else if (item->GetVnum() == UNIQUE_ITEM_HIDE_LEVEL)
			UpdatePacket();
		else if (item->GetVnum() == UNIQUE_ITEM_HORSE_STAMINA)
			SetStopStaminaConsume(true);

		const DWORD& dwVnum = item->GetVnum();

		if (true == CItemVnumHelper::IsRamadanMoonRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_RAMADAN_RING);
		}

		else if (true == CItemVnumHelper::IsHalloweenCandy(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HALLOWEEN_CANDY);
		}

		else if (true == CItemVnumHelper::IsHappinessRing(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_HAPPINESS_RING);
		}

		else if (true == CItemVnumHelper::IsLovePendant(dwVnum))
		{
			this->EffectPacket(SE_EQUIP_LOVE_PENDANT);
		}
		else if (ITEM_UNIQUE == item->GetType() && 0 != item->GetSIGVnum())
		{
			const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(item->GetSIGVnum());
			if (NULL != pGroup)
			{
				const CSpecialAttrGroup* pAttrGroup = ITEM_MANAGER::instance().GetSpecialAttrGroup(pGroup->GetAttrVnum(item->GetVnum()));
				if (NULL != pAttrGroup)
				{
					const std::string& std = pAttrGroup->m_stEffectFileName;
					SpecificEffectPacket(std.c_str());
				}
			}
		}
		#ifdef ENABLE_ACCE_COSTUME_SYSTEM
		else if ((item->GetType() == ITEM_COSTUME) && (item->GetSubType() == COSTUME_ACCE))
			this->EffectPacket(SE_EFFECT_ACCE_EQUIP);
		#endif

		if (item->IsOldMountItem()) // @fixme152
			quest::CQuestManager::instance().SIGUse(GetPlayerID(), quest::QUEST_NO_NPC, item, false);
		#ifdef ENABLE_MOUNT_COSTUME_EX_SYSTEM
		else if (item->IsNewMountItem()) {
			const auto mountVnum = GetPoint(POINT_MOUNT);
			MountVnum(mountVnum);
		}
		#endif

	}

	if (HasPlayerData())
		playerData->m_lastItemEquipTime = get_dword_time();

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	// MT2009_PLUS_MOUNT_ON_EQUIP_V1 (ride): a seal a player puts on (a click in
	// the bag or a drag onto the slot) is ridden at once, as Ctrl+G with the
	// seal in the bag already does (do_ride) - the operator, 28 September:
	// "po kliknieciu na wierzchowiec od razu na niego wchodzil", instead of
	// the seal, the summoned mount beside, and Ctrl+G a second time. Only a
	// player's own action: a login puts the worn seal on through EquipTo, and
	// a bot (no descriptor) keeps getting on and off by its own rules.
	if (GetDesc() && item->IsNewMountItem() && item->IsEquipped() && !IsDead() && !IsStun() &&
			!IsBusyAction() && !IsPolymorphed() && !GetMountVnum())
	{
		CMountSystem* mountSystem = GetMountSystem();
		const DWORD mobVnum = item->GetValue(1);
		if (mountSystem && mobVnum && mountSystem->CountSummoned() == 1)
			mountSystem->Mount(mobVnum, item);
	}
#endif

	return true;
}

LPITEM CHARACTER::FindSpecifyItem(DWORD vnum) const
{
	for (int i = 0; i < GetInventoryMaxCount(); ++i)
		if (GetInventoryItem(i) && GetInventoryItem(i)->GetVnum() == vnum)
			return GetInventoryItem(i);

	return NULL;
}

LPITEM CHARACTER::FindItemByID(DWORD id) const
{
	for (int i=0 ; i < GetInventoryMaxCount(); ++i)
	{
		if (NULL != GetInventoryItem(i) && GetInventoryItem(i)->GetID() == id)
			return GetInventoryItem(i);
	}

	for (int i=BELT_INVENTORY_SLOT_START; i < BELT_INVENTORY_SLOT_END ; ++i)
	{
		if (NULL != GetInventoryItem(i) && GetInventoryItem(i)->GetID() == id)
			return GetInventoryItem(i);
	}

	return NULL;
}

ITEM_COUNT CHARACTER::CountSpecifyItem(DWORD vnum) const
{
	ITEM_COUNT	count = 0;
	LPITEM item;

	for (int i = 0; i < GetInventoryMaxCount(); ++i)
	{
		item = GetInventoryItem(i);
		if (NULL != item && item->GetVnum() == vnum)
		{
			if (m_pkMyShop && m_pkMyShop->IsSellingItem(item->GetID()))
			{
				continue;
			}
			else
			{
				count += item->GetCount();
			}
		}
	}

	return count;
}

void CHARACTER::RemoveSpecifyItem(DWORD vnum, ITEM_COUNT count)
{
	if (0 == count)
		return;

	for (UINT i = 0; i < INVENTORY_MAX_NUM; ++i)
	{
		if (NULL == GetInventoryItem(i))
			continue;

		if (GetInventoryItem(i)->GetVnum() != vnum)
			continue;

		if(m_pkMyShop)
		{
			bool isItemSelling = m_pkMyShop->IsSellingItem(GetInventoryItem(i)->GetID());
			if (isItemSelling)
				continue;
		}

		if (vnum >= 80003 && vnum <= 80007)
			LogManager::instance().GoldBarLog(GetPlayerID(), GetInventoryItem(i)->GetID(), QUEST, "RemoveSpecifyItem");

		if (count >= GetInventoryItem(i)->GetCount())
		{
			count -= GetInventoryItem(i)->GetCount();
			GetInventoryItem(i)->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			GetInventoryItem(i)->SetCount(GetInventoryItem(i)->GetCount() - count);
			return;
		}
	}

	if (count)
		sys_log(0, "CHARACTER::RemoveSpecifyItem cannot remove enough item vnum %u, still remain %d", vnum, count);
}

ITEM_COUNT CHARACTER::CountSpecifyTypeItem(BYTE type) const
{
	ITEM_COUNT	count = 0;

	for (int i = 0; i < GetInventoryMaxCount(); ++i)
	{
		LPITEM pItem = GetInventoryItem(i);
		if (pItem != NULL && pItem->GetType() == type)
		{
			count += pItem->GetCount();
		}
	}

	return count;
}

void CHARACTER::RemoveSpecifyTypeItem(BYTE type, ITEM_COUNT count)
{
	if (0 == count)
		return;

	for (UINT i = 0; i < GetInventoryMaxCount(); ++i)
	{
		if (NULL == GetInventoryItem(i))
			continue;

		if (GetInventoryItem(i)->GetType() != type)
			continue;

		if(m_pkMyShop)
		{
			bool isItemSelling = m_pkMyShop->IsSellingItem(GetInventoryItem(i)->GetID());
			if (isItemSelling)
				continue;
		}

		if (count >= GetInventoryItem(i)->GetCount())
		{
			count -= GetInventoryItem(i)->GetCount();
			GetInventoryItem(i)->SetCount(0);

			if (0 == count)
				return;
		}
		else
		{
			GetInventoryItem(i)->SetCount(GetInventoryItem(i)->GetCount() - count);
			return;
		}
	}
}

void CHARACTER::AutoGiveItem(LPITEM item, bool longOwnerShip)
{
	if (NULL == item)
	{
		sys_err ("NULL point.");
		return;
	}
	if (item->GetOwner())
	{
		sys_err ("item %d 's owner exists!",item->GetID());
		return;
	}

	int cell = GetEmptyInventoryEx(item);
	if (cell != -1)
	{
		item->AddToCharacter(this, TItemPos(item->GetWindowInventoryEx(), cell));

		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot * pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = cell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround (GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif

		if (longOwnerShip)
			item->SetOwnership (this, 300);
		else
			item->SetOwnership (this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}
}

std::pair<bool, LPITEM> CHARACTER::AutoStackItemProto(DWORD dwItemVnum, ITEM_COUNT& bCount)
{
	TItemTable * p = ITEM_MANAGER::instance().GetTable(dwItemVnum);
	if (!p)
		return {false, nullptr};

	// MT2009_PLUS_COR_AUTOSTACK_V1 (server-patches/corautostack): a Cor Draconis
	// joins the stack its owner carries whatever the proto says, as a drag does
	// (IsStackableCorDraconisVnum, server-patches/corstack).
	if (((p->dwFlags & ITEM_FLAG_STACKABLE) || IsStackableCorDraconisVnum(dwItemVnum)) && p->bType != ITEM_BLEND)
	{
		for (int i = 0; i < GetInventoryMaxCount(); ++i)
		{
			LPITEM item = GetInventoryItem(i);
			if (!item)
				continue;

			if (item->GetVnum() == dwItemVnum && FN_check_item_socket(item))
			{
				if (IS_SET(p->dwFlags, ITEM_FLAG_MAKECOUNT))
				{
					if (bCount < p->alValues[1])
						bCount = p->alValues[1];
				}

				auto bCount2 = MIN(item->GetMaxStack() - item->GetCount(), bCount);
				bCount -= bCount2;
				item->SetCount(item->GetCount() + bCount2);

				if (bCount == 0)
					return {true, item};
			}
		}
	}

	return {true, nullptr};
}

LPITEM CHARACTER::AutoStackItem(LPITEM item)
{
	// MT2009_PLUS_COR_AUTOSTACK_V1: a Cor Draconis picked up or given joins the
	// stack already in the bag, even where the proto has ANTI_STACK or lacks
	// STACKABLE - the pickup put every Cor in a stack of its own.
	if ((item->IsStackable() && !IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_STACK)) ||
			IsStackableCorDraconisVnum(item->GetVnum()))
	{
		auto bCount = item->GetCount();

		for (int i = 0; i < GetInventoryMaxCount(); ++i)
		{
			LPITEM item2 = GetInventoryItem(i);
			if (!item2)
				continue;

			if (item == item2)
				continue;

			if (item2->GetVnum() == item->GetVnum())
			{
				int j = 0;
				for (; j < ITEM_SOCKET_MAX_NUM; ++j)
					if (item2->GetSocket(j) != item->GetSocket(j))
						break;

				if (j != ITEM_SOCKET_MAX_NUM)
					continue;

				auto bCount2 = MIN(item2->GetMaxStack() - item2->GetCount(), bCount);
				bCount -= bCount2;
				item2->SetCount(item2->GetCount() + bCount2);

				if (bCount == 0)
				{
					M2_DESTROY_ITEM(item);
					return item2;
				}
			}
		}

		item->SetCount(bCount);
	}

	return nullptr;
}

LPITEM CHARACTER::AutoGiveItem(DWORD dwItemVnum, ITEM_COUNT bCount, int iRarePct, bool bMsg, bool alwaysNewItem)
{
	TItemTable* p = ITEM_MANAGER::instance().GetTable(dwItemVnum);
	if (!p)
		return nullptr;

	ITEM_COUNT original_count = bCount;


	LPITEM foundItem = nullptr;
	if (!alwaysNewItem)
	{
		auto result = AutoStackItemProto(dwItemVnum, bCount);
		auto foundTable = result.first;
		foundItem = result.second;

		if (!foundTable)
			return nullptr;
	}

	DBManager::instance().SendMoneyLog(MONEY_LOG_DROP, dwItemVnum, bCount);

	if (foundItem)
	{
		if (bMsg)
			ChatPacketRecieveItem(this, foundItem, original_count);
		return foundItem;
	}

	LPITEM item = ITEM_MANAGER::instance().CreateItem(dwItemVnum, bCount, 0, true, iRarePct, false, GetPoint(POINT_DROP_RARE));
	if (!item)
	{
		sys_err("cannot create item by vnum %u (name: %s)", dwItemVnum, GetName());
		return NULL;
	}

	if (!alwaysNewItem)
	{
		auto finalItem = AutoStackItem(item); // @fixme316
		if (finalItem)
		{
			if (bMsg)
				ChatPacketRecieveItem(this, finalItem, original_count);
			return finalItem;
		}
	}

	if (item->GetType() == ITEM_BLEND)
	{
		for (int i=0; i < GetInventoryMaxCount(); i++)
		{
			LPITEM inv_item = GetInventoryItem(i);

			if (inv_item == NULL) continue;

			if (inv_item->GetType() == ITEM_BLEND)
			{
				if (inv_item->GetVnum() == item->GetVnum())
				{
					if (inv_item->GetSocket(0) == item->GetSocket(0) &&
							inv_item->GetSocket(1) == item->GetSocket(1) &&
							inv_item->GetSocket(2) == item->GetSocket(2) &&
							inv_item->GetCount() < inv_item->GetMaxStack())
					{
						inv_item->SetCount(inv_item->GetCount() + item->GetCount());
						return inv_item;
					}
				}
			}
		}
	}

	int iEmptyCell = GetEmptyInventoryEx(item);
	if (iEmptyCell != -1)
	{
		if (bMsg)
			ChatPacketRecieveItem(this, item, original_count);

		item->AddToCharacter(this, TItemPos(item->GetWindowInventoryEx(), iEmptyCell));
		LogManager::instance().ItemLog(this, item, "SYSTEM", item->GetName());

		if (item->GetType() == ITEM_USE && item->GetSubType() == USE_POTION)
		{
			TQuickslot * pSlot;

			if (GetQuickslot(0, &pSlot) && pSlot->type == QUICKSLOT_TYPE_NONE)
			{
				TQuickslot slot;
				slot.type = QUICKSLOT_TYPE_ITEM;
				slot.pos = iEmptyCell;
				SetQuickslot(0, slot);
			}
		}
	}
	else
	{
		item->AddToGround(GetMapIndex(), GetXYZ());
#ifdef ENABLE_NEWSTUFF
		item->StartDestroyEvent(g_aiItemDestroyTime[ITEM_DESTROY_TIME_AUTOGIVE]);
#else
		item->StartDestroyEvent();
#endif

		if (IS_SET(item->GetAntiFlag(), ITEM_ANTIFLAG_DROP))
			item->SetOwnership(this, 300);
		else
			item->SetOwnership(this, 60);
		LogManager::instance().ItemLog(this, item, "SYSTEM_DROP", item->GetName());
	}

	sys_log(0,
		"7: %d %d", dwItemVnum, bCount);
	return item;
}

bool CHARACTER::GiveItem(LPCHARACTER victim, TItemPos Cell)
{
	if (!CanHandleItem())
		return false;

	// @fixme150 BEGIN
	if (quest::CQuestManager::instance().GetPCForce(GetPlayerID())->IsRunning() == true)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot take this item if you're using quests"));
		return false;
	}
	// @fixme150 END

	LPITEM item = GetItem(Cell);

	if (item && !item->IsExchanging())
	{
		if (victim->CanReceiveItem(this, item))
		{
			victim->ReceiveItem(this, item);
			return true;
		}
	}

	return false;
}

bool CHARACTER::CanReceiveItem(LPCHARACTER from, LPITEM item) const
{
	if (IsPC())
		return false;

	// TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX
	if (DISTANCE_APPROX(GetX() - from->GetX(), GetY() - from->GetY()) > 2000)
		return false;
	// END_OF_TOO_LONG_DISTANCE_EXCHANGE_BUG_FIX

	switch (GetRaceNum())
	{
		case fishing::CAMPFIRE_MOB:
			if (item->GetType() == ITEM_FISH &&
					(item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
				return true;
			break;

		case fishing::FISHER_MOB:
			if (item->GetType() == ITEM_ROD)
				return true;
			break;

			// BUILDING_NPC
		case BLACKSMITH_WEAPON_MOB:
		case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
			if (item->GetType() == ITEM_WEAPON &&
					item->GetRefinedVnum())
				return true;
			else
				return false;
			break;

		case BLACKSMITH_ARMOR_MOB:
		case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
			if (item->GetType() == ITEM_ARMOR &&
					(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD) &&
					item->GetRefinedVnum())
				return true;
			else
				return false;
			break;

		case BLACKSMITH_ACCESSORY_MOB:
		case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
			if (item->GetType() == ITEM_ARMOR &&
					!(item->GetSubType() == ARMOR_BODY || item->GetSubType() == ARMOR_SHIELD || item->GetSubType() == ARMOR_HEAD) &&
					item->GetRefinedVnum())
				return true;
			else
				return false;
			break;
			// END_OF_BUILDING_NPC

		case BLACKSMITH_MOB:
			if ((item->GetRefinedVnum() || AwakeningSpecialRefineResult(item->GetVnum())) && !IsEliteRefine(item->GetRefineSet())) // MT2009_PLUS_AWAKENING_V1 (smith takes)
			{
				return true;
			}
			else
			{
				return false;
			}

		case BLACKSMITH2_MOB:
			if (IsEliteRefine(item->GetRefineSet()))
			{
				return true;
			}
			else
			{
				return false;
			}

		case ALCHEMIST_MOB:
			if (item->GetRefinedVnum())
				return true;
			break;

		case 20101:
		case 20102:
		case 20103:
			if (item->GetVnum() == ITEM_HORSE_FOOD_1)
			{
				if (IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot feed a dead Horse."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_2 || item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				return false;
			}
		case 20104:
		case 20105:
		case 20106:
			if (item->GetVnum() == ITEM_HORSE_FOOD_2)
			{
				if (IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot feed a dead Horse."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				return false;
			}
			break;
		case 20107:
		case 20108:
		case 20109:
			if (item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				if (IsDead())
				{
					from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot feed a dead Horse."));
					return false;
				}
				return true;
			}
			else if (item->GetVnum() == ITEM_HORSE_FOOD_1 || item->GetVnum() == ITEM_HORSE_FOOD_2)
			{
				return false;
			}
			break;
	}

	//if (IS_SET(item->GetFlag(), ITEM_FLAG_QUEST_GIVE))
	{
		return true;
	}

	return false;
}

bool IsUpgradeableByBlacksmith(LPITEM item)
{
	if (!item)
		return false;

	if (item->GetType() == ITEM_ROD ||
		item->GetType() == ITEM_HERB_KNIFE ||
		item->GetType() == ITEM_PICK)
		return false;

	return item->GetRefinedVnum();
}

void CHARACTER::ReceiveItem(LPCHARACTER from, LPITEM item)
{
	if (IsPC())
		return;

	switch (GetRaceNum())
	{
		case fishing::CAMPFIRE_MOB:
			if (item->GetType() == ITEM_FISH && (item->GetSubType() == FISH_ALIVE || item->GetSubType() == FISH_DEAD))
				fishing::Grill(from, item);
			else
			{
				// TAKE_ITEM_BUG_FIX
				from->SetQuestNPCID(GetVID());
				// END_OF_TAKE_ITEM_BUG_FIX
				quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
			}
			break;

		case fishing::FISHER_MOB:
		{
			if (fishing::RefinableRod(item))
			{
				from->SetRefineNPC(this);
				from->RefineInformation(item->GetCell(), REFINE_TYPE_FISHER);
			}
			else {
				from->SetQuestNPCID(GetVID());
				quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
			}
		}
		break;

		case HERB_BLACKSMITH:
		{
			DWORD current_points = item->GetSocket(0);
			DWORD max_points = item->GetValue(2);
			if (item->GetType() == ITEM_HERB_KNIFE && current_points >= max_points)
			{
				from->SetRefineNPC(this);
				from->RefineInformation(item->GetCell(), REFINE_TYPE_HERB_KNIFE);
			}
			else {
				from->SetQuestNPCID(GetVID());
				quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
			}
		}
		break;

		case PICKAXE_BLACKSMITH:
		{
			DWORD current_points = item->GetSocket(0);
			DWORD max_points = item->GetValue(2);
			if (item->GetType() == ITEM_PICK && current_points >= max_points)
			{
				from->SetRefineNPC(this);
				from->RefineInformation(item->GetCell(), REFINE_TYPE_PICKAXE);
			}
			else {
				from->SetQuestNPCID(GetVID());
				quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
			}
		}
		break;

		// DEVILTOWER_NPC
		case DEVILTOWER_BLACKSMITH_WEAPON_MOB:
		case DEVILTOWER_BLACKSMITH_ARMOR_MOB:
		case DEVILTOWER_BLACKSMITH_ACCESSORY_MOB:
			if (IsUpgradeableByBlacksmith(item) && item->GetRefineSet() != 0 && !IsEliteRefine(item->GetRefineSet()))
			{
				from->SetRefineNPC(this);
				from->RefineInformation(item->GetCell(), REFINE_TYPE_MONEY_ONLY);
			}
			else
			{
				from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be improved."));
			}
			break;
			// END_OF_DEVILTOWER_NPC

		case BLACKSMITH_MOB:
		case BLACKSMITH2_MOB:
		case BLACKSMITH_WEAPON_MOB:
		case BLACKSMITH_ARMOR_MOB:
		case BLACKSMITH_ACCESSORY_MOB:
			if (item->GetRefinedVnum() || AwakeningSpecialRefineResult(item->GetVnum())) // MT2009_PLUS_AWAKENING_V1 (smith window)
			{
				from->SetRefineNPC(this);
				from->RefineInformation(item->GetCell(), REFINE_TYPE_NORMAL);
			}
			else
			{
				from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("This item cannot be improved."));
			}
			break;

		case 20101:
		case 20102:
		case 20103:
		case 20104:
		case 20105:
		case 20106:
		case 20107:
		case 20108:
		case 20109:
			if (item->GetVnum() == ITEM_HORSE_FOOD_1 ||
					item->GetVnum() == ITEM_HORSE_FOOD_2 ||
					item->GetVnum() == ITEM_HORSE_FOOD_3)
			{
				from->FeedHorse();
				from->ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have fed the Horse."));
				item->SetCount(item->GetCount()-1);
				EffectPacket(SE_HPUP_RED);
			}
			break;

		default:
			sys_log(0, "TakeItem %s %d %s", from->GetName(), GetRaceNum(), item->GetName());
			from->SetQuestNPCID(GetVID());
			quest::CQuestManager::instance().TakeItem(from->GetPlayerID(), GetRaceNum(), item);
			break;
	}
}

LPITEM CHARACTER::GetEquipedUniqueItem(DWORD dwItemVnum) const
{
	BYTE uniqueSlots[2] = {WEAR_UNIQUE1 , WEAR_UNIQUE2};
	for (auto slot : uniqueSlots)
	{
		LPITEM u = GetWear(slot);

		if (u && u->GetVnum() == dwItemVnum)
		{
			return u;
		}
	}
	return nullptr;
}

bool CHARACTER::IsEquipUniqueItem(DWORD dwItemVnum) const
{
	if (GetEquipedUniqueItem(dwItemVnum))
		return true;

	if (dwItemVnum == UNIQUE_ITEM_RING_OF_LANGUAGE)
		return IsEquipUniqueItem(UNIQUE_ITEM_RING_OF_LANGUAGE_SAMPLE);

	return false;
}

// CHECK_UNIQUE_GROUP
bool CHARACTER::IsEquipUniqueGroup(DWORD dwGroupVnum) const
{
	{
		LPITEM u = GetWear(WEAR_UNIQUE1);

		if (u && u->GetSpecialGroup() == (int) dwGroupVnum)
			return true;
	}

	{
		LPITEM u = GetWear(WEAR_UNIQUE2);

		if (u && u->GetSpecialGroup() == (int) dwGroupVnum)
			return true;
	}

	return false;
}
// END_OF_CHECK_UNIQUE_GROUP

void CHARACTER::SetRefineMode(int iAdditionalCell)
{
	m_iRefineAdditionalCell = iAdditionalCell;
	m_bUnderRefine = true;
}

void CHARACTER::ClearRefineMode()
{
	m_bUnderRefine = false;
	SetRefineNPC( NULL );
}

LPITEM CHARACTER::GiveItemToPlayerSpecialGroup(DWORD dwVnum, DWORD dwCount, int iRarePct, bool& bSuccess, bool bCreateItemOnly, bool bNotify)
{
	switch (dwVnum)
	{
	case CSpecialItemGroup::GOLD:
		ChangeGold(dwCount);
		bSuccess = true;
		break;
	case CSpecialItemGroup::EXP:
	{
		PointChange(POINT_EXP, dwCount);
		if (bNotify) {
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("A mysterious light comes out of the box."));
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have received %d experience points."), dwCount);
		}
		bSuccess = true;
	}
	break;

	case CSpecialItemGroup::MOB:
	{
		sys_log(0, "CSpecialItemGroup::MOB %d", dwCount);
		int x = GetX() + number(-500, 500);
		int y = GetY() + number(-500, 500);

		LPCHARACTER ch = CHARACTER_MANAGER::instance().SpawnMob(dwCount, GetMapIndex(), x, y, 0, true, -1);
		if (ch)
		{
			ch->SetAggressive();
			if (bNotify)
				ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Look what came out of the box!"));
		}
		bSuccess = true;
	}
	break;
	case CSpecialItemGroup::SLOW:
	{
		sys_log(0, "CSpecialItemGroup::SLOW %d", -(int)dwCount);
		AddAffect(AFFECT_SLOW, POINT_MOV_SPEED, -(int)dwCount, AFF_SLOW, 300, 0, true);
		if (bNotify)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("If you inhale the red smoke coming out of the box, your speed will increase!"));
		bSuccess = true;
	}
	break;
	case CSpecialItemGroup::DRAIN_HP:
	{
		Damage(NULL, dwCount, DAMAGE_TYPE_SYSTEM);
		EffectPacket(SE_FLAME_BLOW);
		if (bNotify)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The box suddenly exploded! You have lost Hit Points (HP)."));
		bSuccess = true;
	}
	break;
	case CSpecialItemGroup::POISON:
	{
		AttackedByPoison(NULL);
		if (bNotify)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("If you inhale the green smoke that is coming out of the box, the poison will spread through your body!"));
		bSuccess = true;
	}
	break;

	case CSpecialItemGroup::MOB_GROUP:
	{
		int sx = GetX() - number(300, 500);
		int sy = GetY() - number(300, 500);
		int ex = GetX() + number(300, 500);
		int ey = GetY() + number(300, 500);
		CHARACTER_MANAGER::instance().SpawnGroup(dwCount, GetMapIndex(), sx, sy, ex, ey, NULL, true);
		if (bNotify)
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Look what came out of the box!"));
		bSuccess = true;
	}
	break;
	default:
	{
		LPITEM pkItem;
		if (bCreateItemOnly)
		{
			pkItem = ITEM_MANAGER::instance().CreateItem(dwVnum, dwCount, 0, true, iRarePct, false, GetPoint(POINT_DROP_RARE));
			if (pkItem)
			{
				bSuccess = true;
				return pkItem;
			}
		}
		else
		{
			pkItem = AutoGiveItem(dwVnum, dwCount, iRarePct);
			if (pkItem)
			{
				if (bNotify)
				{
					if (dwCount > 1)
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Receive: %s - %d"), pkItem->GetName(), dwCount);
					else
						ChatPacket(CHAT_TYPE_INFO, LC_TEXT("The box contains %s."), pkItem->GetName());
				}
				bSuccess = true;
				return pkItem;
			}
		}
	}
	break;
	}
	return nullptr;
}

LPITEM CHARACTER::DropSpecialItemGroup(DWORD dwGroupNum, bool& bSuccess, bool bCreateItemOnly, bool bNotify, int iRarePct)
{
	const CSpecialItemGroup* pGroup = ITEM_MANAGER::instance().GetSpecialItemGroup(dwGroupNum);
	if (!pGroup)
	{
		sys_err("cannot find special item group %d", dwGroupNum);
		return nullptr;
	}

	std::vector <int> idxes;
	int n = pGroup->GetMultiIndex(idxes);

	LPITEM pkItem = NULL;

	for (int i = 0; i < n; i++)
	{
		int idx = idxes[i];
		DWORD dwVnum = pGroup->GetVnum(idx);
		DWORD dwCount = pGroup->GetCount(idx);
		int	iDropRarePct = pGroup->GetRarePct(idx);
		bool isSpecial = pGroup->IsSpecial(idx);

		if (iRarePct >= 0)
			iDropRarePct = MAX(iDropRarePct, iRarePct);

		if (isSpecial)
		{
			pkItem = DropSpecialItemGroup(dwVnum, bSuccess, bCreateItemOnly, bNotify, iRarePct);
		}
		else
			pkItem = GiveItemToPlayerSpecialGroup(dwVnum, dwCount, iDropRarePct, bSuccess, bCreateItemOnly, !bCreateItemOnly && bNotify);
		if (!bSuccess)
			return nullptr;
	}
	return pkItem;
}

// NEW_HAIR_STYLE_ADD
bool CHARACTER::ItemProcess_Hair(LPITEM item, int iDestCell)
{
	if (item->CheckItemUseLevel(GetLevel()) == false)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your level is too low to wear this Hairstyle."));
		return false;
	}

	DWORD hair = item->GetVnum();

	switch (GetJob())
	{
		case JOB_WARRIOR :
			hair -= 72000;
			break;

		case JOB_ASSASSIN :
			hair -= 71250;
			break;

		case JOB_SURA :
			hair -= 70500;
			break;

		case JOB_SHAMAN :
			hair -= 69750;
			break;
#ifdef ENABLE_WOLFMAN_CHARACTER
		case JOB_WOLFMAN:
			break;
#endif
		default :
			return false;
			break;
	}

	if (hair == GetPart(PART_HAIR))
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You already have this Hairstyle."));
		return true;
	}

	item->SetCount(item->GetCount() - 1);

	SetPart(PART_HAIR, hair);
	UpdatePacket();

	return true;
}
// END_NEW_HAIR_STYLE_ADD

bool CHARACTER::ItemProcess_Polymorph(LPITEM item)
{
	if (IsPolymorphed())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You have already transformed."));
		return false;
	}

	if (true == IsRiding())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Not available."));
		return false;
	}

	DWORD dwVnum = item->GetSocket(0);

	if (dwVnum == 0)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("That's the wrong trading item."));
		item->SetCount(item->GetCount()-1);
		return false;
	}

	const CMob* pMob = CMobManager::instance().Get(dwVnum);

	if (pMob == NULL)
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("That's the wrong trading item."));
		item->SetCount(item->GetCount()-1);
		return false;
	}

	switch (item->GetVnum())
	{
		case 70104 :
		case 70105 :
		case 70106 :
		case 70107 :
		case 71093 :
			{
				sys_log(0, "USE_POLYMORPH_BALL PID(%d) vnum(%d)", GetPlayerID(), dwVnum);

				int iPolymorphLevelLimit = MAX(0, 20 - GetLevel() * 3 / 10);
				if (pMob->m_table.bLevel >= GetLevel() + iPolymorphLevelLimit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot transform into a monster who has a higher level than you."));
					return false;
				}

				int iDuration = GetSkillLevel(POLYMORPH_SKILL_ID) == 0 ? 5 : (5 + (5 + (GetSkillLevel(POLYMORPH_SKILL_ID) * 100/40 * 25)/100 ));
				iDuration *= 60;

				DWORD dwBonus = 0;
				

				dwBonus = (200 + GetSkillLevel(POLYMORPH_SKILL_ID) * 100 /40);

				AddAffect(AFFECT_POLYMORPH, POINT_POLYMORPH, dwVnum, AFF_POLYMORPH, iDuration, 0, true);
				AddAffect(AFFECT_POLYMORPH, POINT_ATT_BONUS, dwBonus, AFF_POLYMORPH, iDuration, 0, false);

				item->SetCount(item->GetCount()-1);
			}
			break;

		case 50322:
			{
				sys_log(0, "USE_POLYMORPH_BOOK: %s(%u) vnum(%u)", GetName(), GetPlayerID(), dwVnum);

				if (CPolymorphUtils::instance().PolymorphCharacter(this, item, pMob) == true)
				{
					CPolymorphUtils::instance().UpdateBookPracticeGrade(this, item);
				}
				else
				{
				}
			}
			break;

		default :
			sys_err("POLYMORPH invalid item passed PID(%d) vnum(%d)", GetPlayerID(), item->GetOriginalVnum());
			return false;
	}

	return true;
}

bool CHARACTER::CanDoCube() const
{
	if (m_bIsObserver)	return false;
	if (m_bUnderRefine)	return false;
	if (IsBusy(BUSY_CUBE))		return false;

	return true;
}

bool CHARACTER::UnEquipSpecialRideUniqueItem()
{
	LPITEM Unique1 = GetWear(WEAR_UNIQUE1);
	LPITEM Unique2 = GetWear(WEAR_UNIQUE2);
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	LPITEM MountCostume = GetWear(WEAR_COSTUME_MOUNT);
#endif

	if( NULL != Unique1 )
	{
		if( UNIQUE_GROUP_SPECIAL_RIDE == Unique1->GetSpecialGroup() )
		{
			return UnequipItem(Unique1);
		}
	}

	if( NULL != Unique2 )
	{
		if( UNIQUE_GROUP_SPECIAL_RIDE == Unique2->GetSpecialGroup() )
		{
			return UnequipItem(Unique2);
		}
	}

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
	if (MountCostume)
		return UnequipItem(MountCostume);
#endif

	return true;
}

void CHARACTER::AutoRecoveryItemProcess(const EAffectTypes type)
{
	if (true == IsDead() || true == IsStun())
		return;

	if (false == IsPC())
		return;

	if (AFFECT_AUTO_HP_RECOVERY != type && AFFECT_AUTO_SP_RECOVERY != type)
		return;

	if (NULL != FindAffect(AFFECT_STUN))
		return;

	{
		const DWORD stunSkills[] = { SKILL_CHARGE, SKILL_SWORD_STRIKE, SKILL_SUMMON_LIGHTNING, SKILL_POISON_ARROW };

		for (size_t i=0 ; i < sizeof(stunSkills)/sizeof(DWORD) ; ++i)
		{
			const CAffect* p = FindAffect(stunSkills[i]);

			if (NULL != p && AFF_STUN == p->dwFlag)
				return;
		}
	}

	const CAffect* pAffect = FindAffect(type);
	const size_t idx_of_amount_of_used = 1;
	const size_t idx_of_amount_of_full = 2;

	if (NULL != pAffect)
	{
		LPITEM pItem = FindItemByID(pAffect->dwFlag);

		if (NULL != pItem && true == pItem->GetSocket(0))
		{
			if (!CArenaManager::instance().IsArenaMap(GetMapIndex())
#ifdef ENABLE_NEWSTUFF
				&& !(g_NoPotionsOnPVP && CPVPManager::instance().IsFighting(GetPlayerID()) && !IsAllowedPotionOnPVP(pItem->GetVnum()))
#endif
			)
			{
				const long amount_of_used = pItem->GetSocket(idx_of_amount_of_used);
				const long amount_of_full = pItem->GetSocket(idx_of_amount_of_full);

				const int32_t avail = amount_of_full - amount_of_used;

				int32_t amount = 0;
				int regen_bonus_amount = 0;

				if (AFFECT_AUTO_HP_RECOVERY == type)
				{
					amount = GetMaxHP() - (GetHP() + GetPoint(POINT_HP_RECOVERY));
					regen_bonus_amount = amount * MIN(90, GetPoint(POINT_HP_REGEN)) / 100;
				}
				else if (AFFECT_AUTO_SP_RECOVERY == type)
				{
					amount = GetMaxSP() - (GetSP() + GetPoint(POINT_SP_RECOVERY));
					regen_bonus_amount = amount * MIN(90, GetPoint(POINT_SP_REGEN)) / 100;
				}


				if (amount > 0)
				{
					if (avail > amount)
					{
						const int pct_of_used = amount_of_used * 100 / amount_of_full;
						const int pct_of_will_used = (amount_of_used + amount) * 100 / amount_of_full;

						bool bLog = false;

						if ((pct_of_will_used / 10) - (pct_of_used / 10) >= 1)
							bLog = true;

						int amount_to_use = MAX(1, amount - regen_bonus_amount);

						pItem->SetSocket(idx_of_amount_of_used, amount_of_used + amount_to_use, bLog, "SOCKET_AUTOPOTION");
					}
					else
					{
						amount = avail;

						//ITEM_MANAGER::instance().RemoveItem( pItem );
						// MT2009 edit: do not remove empty potion, just deactivate.
						pItem->Lock(false);
						pItem->SetSocket(idx_of_amount_of_used, amount_of_full, true, "SOCKET_AUTOPOTION");
						pItem->SetSocket(0, false, true, "SOCKET_AUTOPOTION");
						RemoveAffect(const_cast<CAffect*>(pAffect));
					}

					if (AFFECT_AUTO_HP_RECOVERY == type)
					{
						PointChange( POINT_HP_RECOVERY, amount );
						EffectPacket( SE_AUTO_HPUP );
					}
					else if (AFFECT_AUTO_SP_RECOVERY == type)
					{
						PointChange( POINT_SP_RECOVERY, amount );
						EffectPacket( SE_AUTO_SPUP );
					}
				}
			}
			else
			{
				pItem->Lock(false);
				pItem->SetSocket(0, false);
				RemoveAffect( const_cast<CAffect*>(pAffect) );
			}
		}
		else
		{
			RemoveAffect( const_cast<CAffect*>(pAffect) );
		}
	}
}

bool CHARACTER::IsValidItemPosition(TItemPos Pos) const
{
	BYTE window_type = Pos.window_type;
	WORD cell = Pos.cell;

	switch (window_type)
	{
	case RESERVED_WINDOW:
		return false;

	case INVENTORY:
	case EQUIPMENT:
		return cell < (INVENTORY_AND_EQUIP_SLOT_MAX);

	case DRAGON_SOUL_INVENTORY:
		return cell < (DRAGON_SOUL_INVENTORY_MAX_NUM);

	case SAFEBOX:
		if (NULL != m_pkSafebox)
			return m_pkSafebox->IsValidPosition(cell);
		else
			return false;

	case MALL:
		if (NULL != m_pkMall)
			return m_pkMall->IsValidPosition(cell);
		else
			return false;
	default:
		return false;
	}
}

#define VERIFY_MSG(exp, msg)  \
	if (true == (exp)) { \
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT(msg)); \
			return false; \
	}

bool CHARACTER::CanEquipNow(const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell) /*const*/
{
	// playerbot: costumes are off on this line (playerbotify.py, apply_costume_block)
	// -- 2.0.48 fixed a real bug this way: an equipped costume could get stuck
	// un-removable and the character rendered as "just a weapon" (CHANGELOG.md,
	// "Kostiumow nie da sie zalozyc"). A hairstyle passes (apply_costume_hair_
	// allowed) and always did.
	//
	// COSTUME_MOUNT also passes (added 16 September 2026, Mount System
	// integration): that bug was in the visual-part swap this block guards --
	// item.cpp's own GetWearPosition()/part-set switch already treats
	// COSTUME_MOUNT as a no-op there (a mount summons a separate following
	// actor; it never touches the character's own rendered appearance), so
	// the bug this block exists for cannot apply to it.
	//
	// COSTUME_BODY also passes (added 16 September 2026, operator's explicit
	// choice after being told this re-opens the original 2.0.48 bug risk for
	// every body costume, not just the one being added that day -- vnum
	// 41974, "Radiant Lightbearer+" -- not something this code decided on
	// its own). COSTUME_WEAPON also passes, since 17 September 2026 later
	// the same day when ENABLE_WEAPON_COSTUME_SYSTEM actually went live --
	// it renders through its own PART_WEAPON override in item.cpp's
	// equip-time switch, on top of (not instead of) the real weapon's own
	// PART_WEAPON, same non-PART_MAIN reasoning as every other exemption
	// here.
	//
	// COSTUME_ACCE also passes (added 17 September 2026, Szarfa system):
	// this block's original bug was specifically about PART_MAIN getting
	// stuck (a costume overwriting the visible body armor slot) -- Szarfa
	// renders through its own separate PART_ACCE (common/length.h,
	// item.cpp's equip-time part-set switch), never touches PART_MAIN, so
	// the same reasoning that already exempted COSTUME_MOUNT applies here.
	// Operator's report, 17 September 2026: equipping a Szarfa showed
	// "Kostiumy sa na tym serwerze wylaczone." even though
	// ENABLE_ACCE_COSTUME_SYSTEM was already on -- this exemption was
	// simply missing when that flag was enabled earlier the same day.
	// Rewritten 17 September 2026 (Death Ruler weapon costume rollout) to a
	// single runtime OR instead of the 4-way #if/#elif cascade this used to
	// be -- that cascade only ever covered MOUNT+ACCE combinations and
	// silently left COSTUME_WEAPON blocked the moment ENABLE_WEAPON_COSTUME_
	// SYSTEM went live (operator report: equipping a weapon overlay showed
	// "Kostiumy sa na tym serwerze wylaczone." even though the flag was on --
	// same missing-exemption bug as COSTUME_ACCE had earlier the same day).
	// This form does not need a new branch for every future costume subtype.
	bool bCostumeSubtypeExempt = item &&
		(item->GetSubType() == COSTUME_HAIR || item->GetSubType() == COSTUME_BODY
#ifdef ENABLE_MOUNT_COSTUME_SYSTEM
		|| item->GetSubType() == COSTUME_MOUNT
#endif
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
		|| item->GetSubType() == COSTUME_ACCE
#endif
#ifdef ENABLE_WEAPON_COSTUME_SYSTEM
		|| item->GetSubType() == COSTUME_WEAPON
#endif
		);
	if (item && item->GetType() == ITEM_COSTUME && !bCostumeSubtypeExempt)
	{
		ChatPacket(CHAT_TYPE_INFO, "Kostiumy sa na tym serwerze wylaczone.");
		return false;
	}
	if (!PulseManager::Instance().IncreaseCount(GetPlayerID(), ePulse::ItemEquip, std::chrono::milliseconds(500), 5))
	{
		return false;
	}

	if (IsStun())
		return false;

	if (IsBusy())
		return false;

	// Guarded by IsHorseInventory() (window_type == INVENTORY) -- without it
	// this misfired for ANY item whose raw cell number happens to be >=
	// GetInventoryMaxCount() in its OWN window's numbering, e.g. a Dragon
	// Soul stone sitting in DRAGON_SOUL_INVENTORY (0-1343): equipping it
	// would wrongly show "Summon your horse to get access to horse
	// inventory." and refuse. Same pattern already used correctly elsewhere
	// in this file (MoveItem, char_item.cpp:5132/5776).
	if (item && (item->GetType() == ITEM_DS || item->GetType() == ITEM_SPECIAL_DS))
	{
		sys_log(0, "[DS_EQUIP_HORSECHECK] vnum=%u window_type=%u cell=%u GetInventoryMaxCount=%d IsHorseInventory=%d",
			item->GetVnum(), item->GetWindow(), item->GetCell(), (int)GetInventoryMaxCount(),
			IsHorseInventory(item->GetItemPos()) ? 1 : 0);
	}
	if (item && IsHorseInventory(item->GetItemPos()) && item->GetCell() >= GetInventoryMaxCount())
	{
		ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Summon your horse to get access to horse inventory."));
		return false;
	}

	const TItemTable* itemTable = item->GetProto();
	//BYTE itemType = item->GetType();
	//BYTE itemSubType = item->GetSubType();

	switch (GetJob())
	{
		case JOB_WARRIOR:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_WARRIOR)
				return false;
			break;

		case JOB_ASSASSIN:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_ASSASSIN)
				return false;
			break;

		case JOB_SHAMAN:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_SHAMAN)
				return false;
			break;

		case JOB_SURA:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_SURA)
				return false;
			break;
#ifdef ENABLE_WOLFMAN_CHARACTER
		case JOB_WOLFMAN:
			if (item->GetAntiFlag() & ITEM_ANTIFLAG_WOLFMAN)
				return false;
			break;
#endif
	}

	if (item && item->GetVnum() >= 50201 && item->GetVnum() <= 50202)
	{
		if (GetPart(PART_MAIN) < 11903 || GetPart(PART_MAIN) > 11904)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You must wear a wedding dress to equip a bouquet."));
			return false;
		}
	}

	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		long limit = itemTable->aLimits[i].lValue;
		switch (itemTable->aLimits[i].bType)
		{
			case LIMIT_LEVEL:
				if (GetLevel() < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your level is too low to equip this item."));
					return false;
				}
				break;

			case LIMIT_STR:
				if (GetPoint(POINT_ST) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You are not strong enough to equip yourself with this item."));
					return false;
				}
				break;

			case LIMIT_INT:
				if (GetPoint(POINT_IQ) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your intelligence is too low to equip yourself with this item."));
					return false;
				}
				break;

			case LIMIT_DEX:
				if (GetPoint(POINT_DX) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your dexterity is too low to equip yourself with this item."));
					return false;
				}
				break;

			case LIMIT_CON:
				if (GetPoint(POINT_HT) < limit)
				{
					ChatPacket(CHAT_TYPE_INFO, LC_TEXT("Your vitality is too low to equip yourself with this item."));
					return false;
				}
				break;
		}
	}

	if (item->GetWearFlag() & WEARABLE_UNIQUE)
	{
		if ((GetWear(WEAR_UNIQUE1) && GetWear(WEAR_UNIQUE1)->IsSameSpecialGroup(item)) ||
			(GetWear(WEAR_UNIQUE2) && GetWear(WEAR_UNIQUE2)->IsSameSpecialGroup(item)))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot equip this item twice."));
			return false;
		}

		if (marriage::CManager::instance().IsMarriageUniqueItem(item->GetVnum()) &&
			!marriage::CManager::instance().IsMarried(GetPlayerID()))
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You cannot use this item because you are not married."));
			return false;
		}

	}

	if (m_pkFishingEvent || m_pkPreFishingEvent || HasPlayerData() && get_dword_time() - playerData->m_iEndFishingTime < 2500)
	{
		return false;
	}

	return true;
}

bool CHARACTER::CanUnequipNow(const LPITEM item, const TItemPos& srcCell, const TItemPos& destCell, const LPITEM keyItem) /*const*/
{
	if (IsStun())
		return false;

	if (ITEM_BELT == item->GetType())
		VERIFY_MSG(CBeltInventoryHelper::IsExistItemInBeltInventory(this), "You can only discard the belt when there are no longer any items in its inventory.");

	// Removed 17 September 2026 (operator report): this block used to
	// require a wybielacz (70201) to unequip ANY ITEM_COSTUME/COSTUME_HAIR
	// item, including the new Death Ruler helm (vnum 45722) -- costume
	// hairstyles are meant to come off normally via right-click or drag to
	// inventory, same as any other costume. The wybielacz-gated dye/bleach
	// flow it was enforcing belongs to UseItem_Common's NEW_HAIR_STYLE_ADD
	// block (vnum 70201-70206), not to unequipping. ITEM_FLAG_IRREMOVABLE
	// (below) still applies unchanged -- this removal only drops the
	// COSTUME_HAIR-specific wybielacz requirement, nothing else.

	if (item && item->GetVnum() >= 11903 && item->GetVnum() <= 11904)
	{
		if (GetPart(PART_WEAPON) >= 50201 && GetPart(PART_MAIN) <= 50202)
		{
			ChatPacket(CHAT_TYPE_INFO, LC_TEXT("You must unequip bouquet first."));
			return false;
		}
	}

	if (IS_SET(item->GetFlag(), ITEM_FLAG_IRREMOVABLE))
		return false;

	if (m_pkFishingEvent || m_pkPreFishingEvent || HasPlayerData() && get_dword_time() - playerData->m_iEndFishingTime < 2500)
		return false;

	if (m_pkMiningEvent)
		return false;

	if (IsBusyAction())
		return false;

	if (IsBusy())
		return false;

	int pos = GetEmptyInventoryEx(item);
	VERIFY_MSG(-1 == pos, "There isn't enough space in your inventory.");

	return true;
}
void CHARACTER::OnWeaponChange(const LPITEM newItem, const LPITEM oldItem)
{
	if (!newItem || newItem != oldItem)
	{
		if (IsAffectFlag(AFF_SKILL_ENCHANTED_BLADE))
			RemoveAffect(SKILL_ENCHANTED_BLADE);

		if (IsAffectFlag(AFF_SKILL_SWORD_AURA))
			RemoveAffect(SKILL_SWORD_AURA);

		if (auto pkAff = FindAffect(SKILL_FIRE_ARROW))
		{
			if (pkAff->bApplyOn == POINT_ATT_SPECIAL)
				RemoveAffect(pkAff);
		}
	}
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
