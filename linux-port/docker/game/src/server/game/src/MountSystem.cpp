#include "stdafx.h"
#include "../../common/CommonDefines.h"

#ifdef ENABLE_MOUNT_COSTUME_SYSTEM

#include "config.h"
#include "utils.h"
#include "vector.h"
#include "char.h"
#include "sectree_manager.h"
#include "char_manager.h"
#include "mob_manager.h"
#include "MountSystem.h"
#include "packet.h"
#include "item_manager.h"
#include "item.h"
#include "arena.h"

EVENTINFO(mountsystem_event_info)
{
	CMountSystem* pMountSystem;
};

EVENTFUNC(mountsystem_update_event)
{
	mountsystem_event_info* info = dynamic_cast<mountsystem_event_info*>(event->info);
	if (info == NULL)
	{
		sys_err("<mountsystem_update_event> <Factor> Null pointer");
		return 0;
	}

	CMountSystem* pMountSystem = info->pMountSystem;

	if (NULL == pMountSystem)
		return 0;

	pMountSystem->Update(0);
	return PASSES_PER_SEC(1) / 4;
}

///////////////////////////////////////////////////////////////////////////////////////
//  CMountActor
///////////////////////////////////////////////////////////////////////////////////////

CMountActor::CMountActor(LPCHARACTER owner, DWORD vnum)
{
	m_dwVnum = vnum;
	m_dwVID = 0;
	m_dwLastActionTime = 0;

	m_pkChar = 0;
	m_pkOwner = owner;

	m_originalMoveSpeed = 0;

	m_dwSummonItemVID = 0;
	m_dwSummonItemVnum = 0;
}

CMountActor::~CMountActor()
{
	this->Unsummon();
	m_pkOwner = 0;
}

void CMountActor::SetName()
{
	if (0 == m_pkOwner)
		return;

	std::string mountName = m_pkOwner->GetName();

	if (true == IsSummoned())
	{
		mountName += " Mount";

		m_pkChar->SetName(mountName);
	}

	m_name = mountName;
}

// Duration comes from the item's own REAL_TIME_START_FIRST_USE expiry, the same
// mechanism every other time-limited item on this server uses (char_item.cpp,
// UseItem_Common's iLimitRealtimeStartFirstUseFlagIndex block): on first use the
// engine sets socket0 to time(0) + the proto's limit value -- an ABSOLUTE future
// timestamp -- and starts the item's own StartRealTimeExpireEvent(), which
// destroys the item by itself when it elapses. socket0 is therefore never a raw
// duration; the guide this was adapted from subtracted time(0) from it without
// checking either of those things, which underflows (DWORD)0 - time(0) into a
// multi-decade "duration" for an item that was never actually first-used yet.
static bool MountItem_GetRemainingSeconds(LPITEM mountItem, long* pDurationOut)
{
	// MT2009_PLUS_MOUNT_PERMANENT_V1 (server-patches/mountpermanent): a seal whose
	// proto has no real-time limit (the blue and the war mounts, 71115-71128)
	// never gets an expiry in socket0. It is a permanent seal, not an expired
	// one: "Ta pieczec wierzchowca juz wygasla" on a brand new Dzik Wojenny.
	bool bTimedSeal = false;
	for (int i = 0; i < ITEM_LIMIT_MAX_NUM; ++i)
	{
		BYTE bLimit = mountItem->GetProto()->aLimits[i].bType;
		if (LIMIT_REAL_TIME == bLimit || LIMIT_REAL_TIME_START_FIRST_USE == bLimit)
			bTimedSeal = true;
	}
	if (!bTimedSeal)
	{
		*pDurationOut = INFINITE_AFFECT_DURATION;
		return true;
	}

	DWORD dwExpireAt = mountItem->GetSocket(0);
	DWORD dwNow = (DWORD)time(0);

	if (0 == dwExpireAt || dwExpireAt <= dwNow)
	{
		*pDurationOut = 0;
		return false;
	}

	*pDurationOut = (long)(dwExpireAt - dwNow);
	return true;
}

bool CMountActor::Mount(LPITEM mountItem)
{
	if (0 == m_pkOwner)
		return false;

	if (!mountItem)
		return false;

	long lDuration = 0;

	if (false == MountItem_GetRemainingSeconds(mountItem, &lDuration))
	{
		m_pkOwner->ChatPacket(CHAT_TYPE_INFO, "Ta pieczec wierzchowca juz wygasla.");
		return false;
	}

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);

	Unmount();
	// MT2009_PLUS_MOUNT_BONUS_ONCE_V1 (server-patches/mountbonus): Unmount() above clears
	// the seal's bonuses only while GetMountVnum() is set, and a rider who
	// lost the saddle another way (death, a warp) kept them: the next Mount
	// added a second set, and the next a third - +30% experience a ride on
	// a Snow Tiger until MALL_EXPBONUS hit its cap. Cleared every time.
	m_pkOwner->RemoveAffect(AFFECT_MOUNT_BONUS);

	m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, m_dwVnum, AFF_NONE, lDuration, 0, true);

	// The item's own aApplies[] bonuses are applied here, through AFFECT_MOUNT_BONUS,
	// instead of through the normal equip-bonus path (item.cpp's ComputePoints-style
	// loop skips items where IsNewMountItem() is true) -- so a mount seal's stats are
	// counted exactly once, not once from being worn and again from this affect.
	for (int i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		if (mountItem->GetProto()->aApplies[i].bType == POINT_NONE)
			continue;

		m_pkOwner->AddAffect(AFFECT_MOUNT_BONUS, mountItem->GetProto()->aApplies[i].bType, mountItem->GetProto()->aApplies[i].lValue, AFF_NONE, lDuration, 0, false);
	}

	return m_pkOwner->GetMountVnum() == m_dwVnum;
}

void CMountActor::Unmount()
{
	if (0 == m_pkOwner)
		return;

	if (!m_pkOwner->GetMountVnum())
		return;

	m_pkOwner->RemoveAffect(AFFECT_MOUNT);
	m_pkOwner->RemoveAffect(AFFECT_MOUNT_BONUS);
	m_pkOwner->MountVnum(0);

	if (m_pkOwner->IsHorseRiding())
		m_pkOwner->StopRiding();

	if (m_pkOwner->GetHorse())
		m_pkOwner->HorseSummon(false);
}

void CMountActor::Unsummon()
{
	if (true == this->IsSummoned())
	{
		this->SetSummonItem(NULL);

		// MT2009_PLUS_MOUNT_CRASH_FIX_V1 (unsummon), server-patches/mountquickswap (Autor: Digi Rasta):
		// a mount character destroyed from outside stayed in m_pkChar, and
		// destroying it a second time here took the core down. It is ours only
		// while the character manager still knows it under our VID.
		if (NULL != m_pkChar && CHARACTER_MANAGER::instance().Find(m_dwVID) == m_pkChar)
			M2_DESTROY_CHARACTER(m_pkChar);

		m_pkChar = 0;
		m_dwVID = 0;
	}
}

DWORD CMountActor::Summon(LPITEM pSummonItem, bool bSpawnFar)
{
	if (0 == m_pkOwner)
		return 0;

	long x = m_pkOwner->GetX();
	long y = m_pkOwner->GetY();
	long z = m_pkOwner->GetZ();

	if (true == bSpawnFar)
	{
		x += (number(0, 1) * 2 - 1) * number(2000, 2500);
		y += (number(0, 1) * 2 - 1) * number(2000, 2500);
	}
	else
	{
		x += number(-100, 100);
		y += number(-100, 100);
	}

	// MT2009_PLUS_MOUNT_CRASH_FIX_V1 (summon), server-patches/mountquickswap (Autor: Digi Rasta):
	// a mount character destroyed from outside stayed in m_pkChar, and
	// Show() on the freed memory took the core down. One gone meanwhile is
	// forgotten and a new one spawned below.
	if (0 != m_pkChar && CHARACTER_MANAGER::instance().Find(m_dwVID) != m_pkChar)
	{
		m_pkChar = 0;
		m_dwVID = 0;
	}

	if (0 != m_pkChar)
	{
		m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y);
		m_dwVID = m_pkChar->GetVID();

		return m_dwVID;
	}

	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(m_dwVnum, m_pkOwner->GetMapIndex(), x, y, z, false, (int)(m_pkOwner->GetRotation() + 180), false);

	if (0 == m_pkChar)
	{
		sys_err("[CMountActor::Summon] Failed to summon the mount (missing mob_proto row?). (vnum: %d)", m_dwVnum);
		return 0;
	}

	m_pkChar->SetMount();

	m_pkChar->SetEmpire(m_pkOwner->GetEmpire());

	m_dwVID = m_pkChar->GetVID();

	this->SetName();

	this->SetSummonItem(pSummonItem);

	m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y, z);

	return m_dwVID;
}

bool CMountActor::_UpdateFollowAI()
{
	if (0 == m_pkChar || 0 == m_pkChar->m_pkMobData)
		return false;

	if (0 == m_originalMoveSpeed)
	{
		const CMob* mobData = CMobManager::instance().Get(m_dwVnum);

		if (0 != mobData)
			m_originalMoveSpeed = mobData->m_table.sMovingSpeed;
	}

	const float START_FOLLOW_DISTANCE = 300.0f;
	const float RESPAWN_DISTANCE = 4500.f;
	const int APPROACH = 200;

	DWORD currentTime = get_dword_time();

	long ownerX = m_pkOwner->GetX();
	long ownerY = m_pkOwner->GetY();
	long charX = m_pkChar->GetX();
	long charY = m_pkChar->GetY();

	float fDist = DISTANCE_APPROX(charX - ownerX, charY - ownerY);

	if (fDist >= RESPAWN_DISTANCE)
	{
		float fOwnerRot = m_pkOwner->GetRotation() * 3.141592f / 180.f;
		float fx = -APPROACH * cos(fOwnerRot);
		float fy = -APPROACH * sin(fOwnerRot);
		if (m_pkChar->Show(m_pkOwner->GetMapIndex(), ownerX + fx, ownerY + fy))
			return true;
	}

	if (fDist >= START_FOLLOW_DISTANCE)
	{
		m_pkChar->SetNowWalking(false);

		Follow(APPROACH);

		m_pkChar->SetLastAttacked(currentTime);
		m_dwLastActionTime = currentTime;
	}
	else
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);

	return true;
}

bool CMountActor::Update(DWORD deltaTime)
{
	if (0 == m_pkOwner)
		return false;

	// questlua_pc.cpp's pc.set_mount() (an existing quest-granted temporary mount,
	// no actor of its own) writes the same AFFECT_MOUNT/POINT_MOUNT this system
	// uses. If a quest takes that affect over while this actor is actively
	// mounted, the owner's mount vnum stops matching this actor's own -- clean
	// up rather than leave a visual duplicate following someone whose affect
	// now points somewhere else.
	//
	// GetMountVnum()==0 is the ordinary "summoned but not yet ridden" state --
	// equipping the seal (item.cpp -> CHARACTER::MountSummon) only spawns this
	// actor, it does not set AFFECT_MOUNT; do_ride (cmd_general.cpp, already in
	// this codebase) is what calls CMountSystem::Mount() to actually set it,
	// on Ctrl+G/"/ride". Treating 0 as a mismatch here would unsummon the actor
	// on its very first tick after every equip, before the player ever gets to
	// press ride -- only a genuine *different* nonzero vnum means something
	// else took the affect over.
	DWORD dwOwnerMountVnum = m_pkOwner->GetMountVnum();
	if (m_pkOwner->IsDead()
		|| (IsSummoned() && m_pkChar->IsDead())
		|| (0 != dwOwnerMountVnum && dwOwnerMountVnum != m_dwVnum)
		|| NULL == ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID())
		|| ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID())->GetOwner() != this->GetOwner())
	{
		this->Unsummon();
		return true;
	}

	if (this->IsSummoned())
		return this->_UpdateFollowAI();

	return true;
}

bool CMountActor::Follow(float fMinDistance)
{
	if (!m_pkOwner || !m_pkChar)
		return false;

	float fOwnerX = m_pkOwner->GetX();
	float fOwnerY = m_pkOwner->GetY();

	float fMountX = m_pkChar->GetX();
	float fMountY = m_pkChar->GetY();

	float fDist = DISTANCE_SQRT(fOwnerX - fMountX, fOwnerY - fMountY);
	if (fDist <= fMinDistance)
		return false;

	m_pkChar->SetRotationToXY(fOwnerX, fOwnerY);

	float fx, fy;

	float fDistToGo = fDist - fMinDistance;
	GetDeltaByDegree(m_pkChar->GetRotation(), fDistToGo, &fx, &fy);

	if (!m_pkChar->Goto((int)(fMountX + fx + 0.5f), (int)(fMountY + fy + 0.5f)))
		return false;

	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);

	return true;
}

void CMountActor::SetSummonItem(LPITEM pItem)
{
	if (NULL == pItem)
	{
		m_dwSummonItemVID = 0;
		m_dwSummonItemVnum = 0;
		return;
	}

	m_dwSummonItemVID = pItem->GetVID();
	m_dwSummonItemVnum = pItem->GetVnum();
}

///////////////////////////////////////////////////////////////////////////////////////
//  CMountSystem
///////////////////////////////////////////////////////////////////////////////////////

CMountSystem::CMountSystem(LPCHARACTER owner)
{
	m_pkOwner = owner;
	m_dwUpdatePeriod = 400;
	m_dwLastUpdateTime = 0;
	m_pkMountSystemUpdateEvent = NULL;
}

CMountSystem::~CMountSystem()
{
	Destroy();
}

void CMountSystem::Destroy()
{
	// Cancel the periodic update event BEFORE tearing down the actors it walks --
	// event_cancel(&ptr) also nulls m_pkMountSystemUpdateEvent, so a Destroy()
	// called twice (owner destructor + an earlier explicit Destroy()) is safe:
	// the second call finds nothing left to cancel and an empty map.
	event_cancel(&m_pkMountSystemUpdateEvent);

	for (TMountActorMap::iterator iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		CMountActor* mountActor = iter->second;

		if (0 != mountActor)
			delete mountActor;
	}

	m_mountActorMap.clear();
}

bool CMountSystem::Update(DWORD deltaTime)
{
	bool bResult = true;

	DWORD currentTime = get_dword_time();

	if (m_dwUpdatePeriod > currentTime - m_dwLastUpdateTime)
		return true;

	std::vector<CMountActor*> v_garbageActor;

	for (TMountActorMap::iterator iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		CMountActor* mountActor = iter->second;

		if (0 != mountActor && mountActor->IsSummoned())
		{
			LPCHARACTER pMount = mountActor->GetCharacter();

			// MT2009_PLUS_MOUNT_CRASH_FIX_V1 (update), server-patches/mountquickswap (Autor: Digi Rasta):
			// pMount->GetVID() read a character destroyed from outside. The VID the
			// actor kept is looked up and the pointer only compared.
			if (NULL == pMount || CHARACTER_MANAGER::instance().Find(mountActor->GetVID()) != pMount)
				v_garbageActor.push_back(mountActor);
			else
				bResult = bResult && mountActor->Update(deltaTime);
		}
	}

	for (std::vector<CMountActor*>::iterator it = v_garbageActor.begin(); it != v_garbageActor.end(); ++it)
		DeleteMount(*it);

	m_dwLastUpdateTime = currentTime;

	return bResult;
}

void CMountSystem::DeleteMount(DWORD mobVnum)
{
	TMountActorMap::iterator iter = m_mountActorMap.find(mobVnum);

	if (m_mountActorMap.end() == iter)
	{
		sys_err("[CMountSystem::DeleteMount] Can't find mount on my list (VNUM: %d)", mobVnum);
		return;
	}

	CMountActor* mountActor = iter->second;

	if (0 == mountActor)
		sys_err("[CMountSystem::DeleteMount] Null Pointer (mountActor)");
	else
		delete mountActor;

	m_mountActorMap.erase(iter);
}

void CMountSystem::DeleteMount(CMountActor* mountActor)
{
	for (TMountActorMap::iterator iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		if (iter->second == mountActor)
		{
			delete mountActor;
			m_mountActorMap.erase(iter);
			return;
		}
	}

	sys_err("[CMountSystem::DeleteMount] Can't find mountActor(0x%x) on my list(size: %d)", mountActor, (int)m_mountActorMap.size());
}

void CMountSystem::Unsummon(DWORD vnum, bool bDeleteFromList)
{
	CMountActor* actor = this->GetByVnum(vnum);

	if (0 == actor)
	{
		sys_err("[CMountSystem::Unsummon(%d)] Null Pointer (actor)", vnum);
		return;
	}
	actor->Unsummon();

	if (true == bDeleteFromList)
		this->DeleteMount(actor);

	bool bActive = false;
	for (TMountActorMap::iterator it = m_mountActorMap.begin(); it != m_mountActorMap.end(); ++it)
		bActive |= it->second->IsSummoned();

	if (false == bActive)
		event_cancel(&m_pkMountSystemUpdateEvent);
}

void CMountSystem::Summon(DWORD mobVnum, LPITEM pSummonItem, bool bSpawnFar)
{
	if (0 == mobVnum || NULL == pSummonItem)
		return;

	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (0 == mountActor)
	{
		mountActor = M2_NEW CMountActor(m_pkOwner, mobVnum);
		m_mountActorMap.insert(std::make_pair(mobVnum, mountActor));
	}

	DWORD mountVID = mountActor->Summon(pSummonItem, bSpawnFar);

	if (!mountVID)
		sys_err("[CMountSystem::Summon(%d)] mount actor failed to spawn (item id: %d)", mobVnum, pSummonItem->GetID());

	if (NULL == m_pkMountSystemUpdateEvent)
	{
		mountsystem_event_info* info = AllocEventInfo<mountsystem_event_info>();

		info->pMountSystem = this;

		m_pkMountSystemUpdateEvent = event_create(mountsystem_update_event, info, PASSES_PER_SEC(1) / 4);
	}
}

void CMountSystem::Mount(DWORD mobVnum, LPITEM mountItem)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		sys_err("[CMountSystem::Mount] Null Pointer (mountActor)");
		return;
	}

	if (!mountItem)
		return;

	this->Unsummon(mobVnum, false);
	mountActor->Mount(mountItem);
}

void CMountSystem::Unmount(DWORD mobVnum)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		sys_err("[CMountSystem::Unmount] Null Pointer (mountActor)");
		return;
	}

	if (0 != m_pkOwner)
	{
		if (LPITEM pSummonItem = m_pkOwner->GetWear(WEAR_COSTUME_MOUNT))
			this->Summon(mobVnum, pSummonItem, false);
	}

	mountActor->Unmount();
}

CMountActor* CMountSystem::GetByVID(DWORD vid) const
{
	for (TMountActorMap::const_iterator iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		CMountActor* mountActor = iter->second;

		if (0 == mountActor)
		{
			sys_err("[CMountSystem::GetByVID(%d)] Null Pointer (mountActor)", vid);
			continue;
		}

		if (mountActor->GetVID() == vid)
			return mountActor;
	}

	return 0;
}

CMountActor* CMountSystem::GetByVnum(DWORD vnum) const
{
	TMountActorMap::const_iterator iter = m_mountActorMap.find(vnum);

	return (m_mountActorMap.end() != iter) ? iter->second : 0;
}

size_t CMountSystem::CountSummoned() const
{
	size_t count = 0;

	for (TMountActorMap::const_iterator iter = m_mountActorMap.begin(); iter != m_mountActorMap.end(); ++iter)
	{
		CMountActor* mountActor = iter->second;

		if (0 != mountActor && mountActor->IsSummoned())
			++count;
	}

	return count;
}

///////////////////////////////////////////////////////////////////////////////////////
//  CHARACTER hooks -- declared in char.h, defined here rather than in char.cpp to
//  keep that file's own diff to the constructor/destructor plumbing only.
///////////////////////////////////////////////////////////////////////////////////////

// map 113 is the OX Quiz event map (see input_login.cpp / char_item.cpp, which
// already gate other things on this same map on this server) -- not a magic
// number carried over from the guide this was adapted from.
static const long MOUNT_FORBIDDEN_MAP_OX_QUIZ = 113;

void CHARACTER::MountSummon(LPITEM mountItem)
{
	if (IsPolymorphed())
	{
		ChatPacket(CHAT_TYPE_INFO, "Nie mozna przywolac wierzchowca w tej postaci.");
		return;
	}

	if (GetMapIndex() == MOUNT_FORBIDDEN_MAP_OX_QUIZ)
		return;

	if (CArenaManager::instance().IsArenaMap(GetMapIndex()))
		return;

	CMountSystem* mountSystem = GetMountSystem();

	if (!mountSystem || !mountItem)
		return;

	DWORD mobVnum = mountItem->GetValue(1);

	if (0 == mobVnum)
	{
		sys_err("[CHARACTER::MountSummon] %s: item %u has no mob vnum in value1", GetName(), mountItem->GetVnum());
		return;
	}

	if (IsHorseRiding())
		StopRiding();

	if (GetHorse())
		HorseSummon(false);

	mountSystem->Summon(mobVnum, mountItem, false);
}

void CHARACTER::MountUnsummon(LPITEM mountItem)
{
	CMountSystem* mountSystem = GetMountSystem();

	if (!mountSystem || !mountItem)
		return;

	DWORD mobVnum = mountItem->GetValue(1);

	if (0 == mobVnum)
		return;

	if (GetMountVnum() == mobVnum)
		mountSystem->Unmount(mobVnum);

	mountSystem->Unsummon(mobVnum);
}

void CHARACTER::CheckMount()
{
	CMountSystem* mountSystem = GetMountSystem();
	LPITEM mountItem = GetWear(WEAR_COSTUME_MOUNT);

	if (!mountSystem || !mountItem)
		return;

	DWORD mobVnum = mountItem->GetValue(1);

	if (0 == mobVnum)
		return;

	if (mountSystem->CountSummoned() == 0)
		mountSystem->Summon(mobVnum, mountItem, false);
}

bool CHARACTER::IsRidingMount()
{
	return (GetWear(WEAR_COSTUME_MOUNT) != NULL || FindAffect(AFFECT_MOUNT) != NULL);
}

#endif	// ENABLE_MOUNT_COSTUME_SYSTEM
