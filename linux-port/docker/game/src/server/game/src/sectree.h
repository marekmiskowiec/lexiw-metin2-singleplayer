#ifndef __INC_SECTREE_H__
#define __INC_SECTREE_H__

#include "entity.h"

enum ESectree
{
	SECTREE_SIZE	= 6400,
	SECTREE_HALF_SIZE	= 3200,
	CELL_SIZE		= 50
};

typedef struct sectree_coord
{
	unsigned            x : 16;
	unsigned            y : 16;
} SECTREE_COORD;

typedef union sectreeid
{
	DWORD		package;
	SECTREE_COORD	coord;
} SECTREEID;

enum
{
	ATTR_BLOCK = (1 << 0),
	ATTR_WATER = (1 << 1),
	ATTR_BANPK = (1 << 2),
	ATTR_OBJECT = (1 << 7),
};

// MT2009_PLUS_ENTITY_SNAPSHOT_V1 (collector) (server-patches/entitysnapshot):
// the snapshot below is walked after it is taken, and a functor can destroy
// characters still waiting in it - a splash skill's blow kills a monster,
// its kill trigger runs d.purge_area() and the rest of the wave is freed
// while FuncSplashDamage still has them in the list. The next blow then
// landed on freed memory: Damage -> BeginFight -> SetNextStatePulse put the
// dead pointer back into the state list and the core died in
// CHARACTER_MANAGER::Update a few seconds later (ch1-game2, Arezzo dungeons,
// 1 October). Each character is remembered with its VID and walked only
// while the character manager still holds that VID for that pointer.
DWORD SECTREE_SnapshotKey(LPENTITY entity);
bool SECTREE_SnapshotAlive(LPENTITY entity, DWORD key);

struct FCollectEntity {
	void operator()(LPENTITY entity) {
		result.emplace_back(entity);
		keys.emplace_back(SECTREE_SnapshotKey(entity));
	}
	template<typename F>
	void ForEach(F& f) {
		for (size_t i = 0; i < result.size(); ++i) {
			LPENTITY entity = result[i];
			if (!SECTREE_SnapshotAlive(entity, keys[i]))
				continue;
			f(entity);
		}
	}
	typedef std::vector<LPENTITY> ListType;
	ListType result; // list collected
	std::vector<DWORD> keys; // the VID of each character in result, 0 for the rest
};

class CAttribute;

class SECTREE
{
	public:
		friend class SECTREE_MANAGER;
		friend class SECTREE_MAP;

		template <class _Func> LPENTITY	find_if (_Func & func) const
		{
#ifdef __clang__
			LPSECTREE_LIST::const_iterator it_tree = m_neighbor_list.begin();
#else
			LPSECTREE_LIST::iterator it_tree = m_neighbor_list.begin();
#endif

			while (it_tree != m_neighbor_list.end())
			{
				ENTITY_SET::iterator it_entity = (*it_tree)->m_set_entity.begin();

				while (it_entity != (*it_tree)->m_set_entity.end())
				{
					if (func(*it_entity))
						return (*it_entity);

					++it_entity;
				}

				++it_tree;
			}

			return NULL;
		}

		template <class _Func> void ForEachAround(_Func & func)
		{
			// <Factor> Using snapshot copy to avoid side-effects
			FCollectEntity collector;
#ifdef __clang__
			LPSECTREE_LIST::const_iterator it = m_neighbor_list.begin();
#else
			LPSECTREE_LIST::iterator it = m_neighbor_list.begin();
#endif
			for ( ; it != m_neighbor_list.end(); ++it)
			{
				LPSECTREE sectree = *it;
				sectree->for_each_entity(collector);
			}
			collector.ForEach(func);
		}

		template <class _Func> void for_each_for_find_victim(_Func & func)
		{
#ifdef __clang__
			LPSECTREE_LIST::const_iterator it_tree = m_neighbor_list.begin();
#else
			LPSECTREE_LIST::iterator it_tree = m_neighbor_list.begin();
#endif

			while (it_tree != m_neighbor_list.end())
			{
				if ( (*(it_tree++))->for_each_entity_for_find_victim(func) )
					return;
			}
		}
		template <class _Func> bool for_each_entity_for_find_victim(_Func & func)
		{
			itertype(m_set_entity) it = m_set_entity.begin();

			while (it != m_set_entity.end())
			{
				if ( func(*it++) )
					return true;
			}
			return false;
		}

	public:
		SECTREE();
		~SECTREE();

		void				Initialize();
		void				Destroy();

		SECTREEID			GetID();

		bool				InsertEntity(LPENTITY ent);
		void				RemoveEntity(LPENTITY ent);

		void				SetRegenEvent(LPEVENT event);
		bool				Regen();

		void				IncreasePC();
		void				DecreasePC();

		void				BindAttribute(CAttribute * pkAttribute);

		CAttribute *			GetAttributePtr() { return m_pkAttribute; }

		DWORD				GetAttribute(long x, long y);
		bool				IsAttr(long x, long y, DWORD dwFlag);
		bool				IsNearAttr(long x, long y, DWORD dwFlag);

		void				CloneAttribute(LPSECTREE tree);

		int				GetEventAttribute(long x, long y);

		void				SetAttribute(DWORD x, DWORD y, DWORD dwAttr);
		void				RemoveAttribute(DWORD x, DWORD y, DWORD dwAttr);

	private:
		template <class _Func> void for_each_entity(_Func & func)
		{
			itertype(m_set_entity) it = m_set_entity.begin();
			for ( ; it != m_set_entity.end(); ++it) {
				LPENTITY entity = *it;
				// <Factor> Sanity check
				if (entity->GetSectree() != this) {
					sys_err("<Factor> SECTREE-ENTITY relationship mismatch");
					m_set_entity.erase(it);
					continue;
				}
				func(entity);
			}
		}

		SECTREEID			m_id;
		ENTITY_SET			m_set_entity;
		LPSECTREE_LIST			m_neighbor_list;
		int				m_iPCCount;
		bool				isClone;

		CAttribute *			m_pkAttribute;
};

#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
