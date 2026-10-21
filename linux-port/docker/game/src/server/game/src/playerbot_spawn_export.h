// LEXIW_SPAWN_TIMERS_V1: what the world's important spawn points hold now,
// for the panel's "Timery spawnów" page.
//
// Every ten seconds each core writes /opt/m2spool/world_spawns.<channel>.<port>.tsv:
//     R  map  sx sy ex ey  type vnum  count max  next_s  (a respawn line whose
//        monsters include a boss, a mini boss, a Metin stone, an ore vein or a
//        herb - the regen's own is_boss_or_stone; area in local metres; next_s
//        the seconds to its next tick, -1 for none; then the vnums it may
//        put down, comma-separated)
//     A  map  vnum  x y  vid  (one such monster standing now, local metres)
// The respawn of a line ticks on its own clock (regen_event): at each tick a
// dead one is put back, a living one left - so "next_s" is when an empty
// point will be filled at the latest.
//
// An implementation fragment in the sense playerbot_types.h describes:
// included once, in playerbot_manager.cpp, after playerbot_dancer.h.

extern LPREGEN regen_list;

namespace
{
	const DWORD PLAYERBOT_SPAWN_EXPORT_MS = 10000;
	DWORD s_dwPlayerBotNextSpawnExport = 0;

	void CollectPlayerBotRegenMembers(LPREGEN regen, std::set<DWORD>& out)
	{
		if (regen->type == REGEN_TYPE_MOB)
		{
			out.insert(regen->vnum);
			return;
		}
		std::vector<DWORD> groups;
		if (regen->type == REGEN_TYPE_GROUP)
			groups.push_back(regen->vnum);
		else if (regen->type == REGEN_TYPE_GROUP_GROUP)
		{
			const std::vector<DWORD>* gg = CMobManager::instance().GetGroupGroupMembers(regen->vnum);
			if (gg)
				groups = *gg;
		}
		for (size_t i = 0; i < groups.size(); ++i)
		{
			CMobGroup* group = CMobManager::instance().GetGroup(groups[i]);
			if (!group)
				continue;
			const std::vector<DWORD>& members = group->GetMemberVector();
			out.insert(members.begin(), members.end());
		}
	}

	void ExportPlayerBotWorldSpawns(DWORD dwNow)
	{
		if (dwNow < s_dwPlayerBotNextSpawnExport)
			return;
		s_dwPlayerBotNextSpawnExport = dwNow + PLAYERBOT_SPAWN_EXPORT_MS;

		char path[128], target[128];
		// One file per core: a channel runs several (each its maps), by its port.
		snprintf(target, sizeof(target), "/opt/m2spool/world_spawns.%u.%u.tsv", (unsigned int)g_bChannel, (unsigned int)mother_port);
		snprintf(path, sizeof(path), "%s.new", target);
		FILE* fp = fopen(path, "w");
		if (!fp)
			return;
		fprintf(fp, "T\t%ld\n", (long)time(0));

		// map -> the monsters its lines may put down
		std::map<long, std::set<DWORD> > wanted;
		for (LPREGEN regen = regen_list; regen; regen = regen->next)
		{
			if (!regen->is_boss_or_stone)
				continue;
			PIXEL_POSITION base;
			if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(regen->lMapIndex, base))
				continue;
			std::set<DWORD> members;
			CollectPlayerBotRegenMembers(regen, members);
			std::string list;
			for (std::set<DWORD>::const_iterator m = members.begin(); m != members.end(); ++m)
			{
				char one[16];
				snprintf(one, sizeof(one), list.empty() ? "%u" : ",%u", *m);
				list += one;
			}
			const long next = regen->event ? event_time(regen->event) / PASSES_PER_SEC(1) : -1;
			fprintf(fp, "R\t%ld\t%ld\t%ld\t%ld\t%ld\t%d\t%d\t%d\t%d\t%ld\t%s\n", regen->lMapIndex,
					(regen->sx - base.x) / 100, (regen->sy - base.y) / 100, (regen->ex - base.x) / 100,
					(regen->ey - base.y) / 100, regen->type, regen->vnum, regen->count, regen->max_count, next,
					list.empty() ? "-" : list.c_str());
			wanted[regen->lMapIndex].insert(members.begin(), members.end());
		}

		// Every character of the core, not GetCharactersByRaceNum: that one
		// knows only the races a quest registered (veins, herbs), never the
		// Metins and bosses.
		const std::unordered_map<DWORD, LPCHARACTER>& all = CHARACTER_MANAGER::instance().GetCharacterVIDMap();
		for (std::unordered_map<DWORD, LPCHARACTER>::const_iterator c = all.begin(); c != all.end(); ++c)
		{
			LPCHARACTER ch = c->second;
			if (!ch || ch->IsPC() || ch->IsDead() || !ch->GetSectree())
				continue;
			const DWORD race = ch->GetRaceNum();
			std::map<long, std::set<DWORD> >::const_iterator w = wanted.find(ch->GetMapIndex());
			if (w == wanted.end() || !w->second.count(race))
				continue;
			PIXEL_POSITION base;
			if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(ch->GetMapIndex(), base))
				continue;
			fprintf(fp, "A\t%ld\t%u\t%ld\t%ld\t%u\n", ch->GetMapIndex(), race, (ch->GetX() - base.x) / 100,
					(ch->GetY() - base.y) / 100, (DWORD)ch->GetVID());
		}
		fclose(fp);
		chmod(path, 0664);
		rename(path, target);
	}
}
