// LEXIW_DANCE_SHOW_V1: "Show tanca" - the operator's own troupe of dancers.
//
// A handful of characters that belong to nobody's population: the operator
// names them, picks their class and level in the Seban panel ("Show tanca"),
// gathers them beside one of his own characters in a formation and starts one
// dance for all of them in the same server tick, to film it and set it to a
// song. They never hunt, trade, travel or answer the population's calls.
//
// The roster is player.playerbot_dancer (the panel writes name, class, level
// and slot; this file claims an identity for each row and writes its pid
// back). An identity is one of the seeded playerbot_NNN characters that has
// never played, picked the way a companion is (PickPlayerBotSidekickIdentity's
// guards), renamed while it is out of every world and of the db core's cache,
// and from then on spawned only here, through SpawnSidekick's path - Spawn
// lets nothing else start it (IsPlayerBotDancerPID), the population's
// schedules step over it, and the bot tick only keeps it standing still.
//
// The show itself is /opt/m2spool/playerbot_dance.tsv, "key value" lines the
// panel rewrites for every button:
//     command  gather | start | stop
//     owner    pid of the character the troupe stands beside
//     count    how many dancers (the first N of the roster, by slot)
//     formation row | rows | circle
//     dance    1..6 (the client's dance1..dance6)
//     repeat   seconds between two starts of the dance, 0 = once
//     stamp    anything new each time the panel writes the file
// and what this core sees of it goes back to the panel in
// /opt/m2spool/playerbot_dance.status. Only the core the owner stands on acts.
//
// An implementation fragment in the sense playerbot_types.h describes: include
// it exactly once, after playerbot_sidekick.h, whose identity pick, spawn path
// and placement it uses.

namespace
{
	const char* const PLAYERBOT_DANCE_SHOW_PATH = "/opt/m2spool/playerbot_dance.tsv";
	const char* const PLAYERBOT_DANCE_STATUS_PATH = "/opt/m2spool/playerbot_dance.status";
	const DWORD PLAYERBOT_DANCE_RELOAD_MS = 5000;
	const DWORD PLAYERBOT_DANCE_SPAWN_RETRY_MS = 10000;
	// The first beat waits this long after "start", so every dancer of a fresh
	// gathering has had its position sent before the music starts.
	const DWORD PLAYERBOT_DANCE_COUNTDOWN_MS = 2000;
	// Owner gone from this core's world this long: the troupe logs out.
	const DWORD PLAYERBOT_DANCE_OWNER_GONE_MS = 20000;
	// A renamed identity must be out of the db core's cache (g_iLogoutSeconds,
	// ten minutes) or it loads under its old name.
	const int PLAYERBOT_DANCE_RENAME_IDLE_MINUTES = 11;
	const int PLAYERBOT_DANCE_MAX = 40;
	const long PLAYERBOT_DANCE_SPACING = 250;

	struct TPlayerBotDancer
	{
		DWORD dwRow;
		int iSlot;
		std::string strName;
		BYTE bJob;
		BYTE bLevel;
		DWORD dwPid;
		// The look the panel set (item vnums, 0 = leave what it wears):
		// weapon, body armour, costume, hairstyle, weapon skin (after the weapon:
		// a skin is worn over a weapon of its kind).
		DWORD adwLook[5];
	};

	struct TPlayerBotDanceShow
	{
		std::string strCommand;
		DWORD dwOwner;
		int iCount;
		std::string strFormation;
		int iDance;
		DWORD dwRepeatMs;
		std::string strStamp;
		bool bQuiet;
	};

	std::vector<TPlayerBotDancer> s_vecPlayerBotDancers;
	std::set<DWORD> s_setPlayerBotDancerPids;
	TPlayerBotDanceShow s_kPlayerBotDanceShow;
	time_t s_tPlayerBotDanceShowMtime = 0;
	DWORD s_dwPlayerBotDancerNextLoad = 0;
	DWORD s_dwPlayerBotDanceNextStatus = 0;
	DWORD s_dwPlayerBotDancerNextClaim = 0;
	DWORD s_dwPlayerBotDancerNextManage = 0;
	bool s_bPlayerBotDancerTable = false;
	// Per dancer: the stamp of the show it was placed for, and the next spawn try.
	std::map<DWORD, std::string> s_mapPlayerBotDancerPlaced;
	std::map<DWORD, DWORD> s_mapPlayerBotDancerNextSpawn;
	// The beat: 0 until "start" is seen for a stamp, then the time of the next start.
	std::string s_strPlayerBotDanceBeatStamp;
	DWORD s_dwPlayerBotDanceNextBeat = 0;
	DWORD s_dwPlayerBotDanceBeats = 0;
	time_t s_tPlayerBotDanceLastBeat = 0;
	DWORD s_dwPlayerBotDanceOwnerSeen = 0;
	std::string s_strPlayerBotDanceNote;

	bool IsPlayerBotDancerPID(DWORD pid)
	{
		return pid != 0 && s_setPlayerBotDancerPids.find(pid) != s_setPlayerBotDancerPids.end();
	}

	bool IsPlayerBotDancerNameValid(const std::string& name)
	{
		if (name.size() < 2 || name.size() > 24)
			return false;
		for (size_t i = 0; i < name.size(); ++i)
		{
			const char c = name[i];
			if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '[' || c == ']'))
				return false;
		}
		return true;
	}

	long long PlayerBotDanceExec(const char* query)
	{
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get())
			return -1;
		return (long long)msg->Get()->uiAffectedRows;
	}

	void SetPlayerBotDancerStatus(DWORD row, const char* status)
	{
		char query[256];
		snprintf(query, sizeof(query), "UPDATE player.playerbot_dancer SET status='%s' WHERE id=%u", status, row);
		PlayerBotDanceExec(query);
	}

	void LoadPlayerBotDancers()
	{
		if (!s_bPlayerBotDancerTable)
		{
			s_bPlayerBotDancerTable = PlayerBotDanceExec(
					"CREATE TABLE IF NOT EXISTS player.playerbot_dancer ("
					"id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY, "
					"slot SMALLINT NOT NULL DEFAULT 0, "
					"name VARCHAR(24) NOT NULL, "
					"job TINYINT UNSIGNED NOT NULL DEFAULT 0, "
					"level TINYINT UNSIGNED NOT NULL DEFAULT 1, "
					"pid INT UNSIGNED NULL, "
					"status VARCHAR(64) NOT NULL DEFAULT '', "
					"created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, "
					"UNIQUE KEY pid (pid)) ENGINE=InnoDB") >= 0;
			if (!s_bPlayerBotDancerTable)
			{
				sys_err("PLAYERBOT_DANCE: no dancer table");
				return;
			}
			// The look of each dancer (LEXIW_DANCE_SHOW_V1, 20 October): added to
			// a table that may predate it.
			PlayerBotDanceExec("ALTER TABLE player.playerbot_dancer "
					"ADD COLUMN IF NOT EXISTS weapon INT UNSIGNED NOT NULL DEFAULT 0, "
					"ADD COLUMN IF NOT EXISTS armor INT UNSIGNED NOT NULL DEFAULT 0, "
					"ADD COLUMN IF NOT EXISTS costume INT UNSIGNED NOT NULL DEFAULT 0, "
					"ADD COLUMN IF NOT EXISTS hair INT UNSIGNED NOT NULL DEFAULT 0, "
					"ADD COLUMN IF NOT EXISTS skin INT UNSIGNED NOT NULL DEFAULT 0");
		}
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"SELECT id, slot, name, job, level, IFNULL(pid,0), weapon, armor, costume, hair, skin "
				"FROM player.playerbot_dancer ORDER BY slot, id"));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
			return;
		std::vector<TPlayerBotDancer> dancers;
		std::set<DWORD> pids;
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			TPlayerBotDancer d;
			unsigned int job = 0, level = 1;
			d.dwRow = 0;
			d.iSlot = 0;
			d.dwPid = 0;
			str_to_number(d.dwRow, row[0]);
			str_to_number(d.iSlot, row[1]);
			d.strName = row[2] ? row[2] : "";
			str_to_number(job, row[3]);
			str_to_number(level, row[4]);
			str_to_number(d.dwPid, row[5]);
			for (int k = 0; k < 5; ++k)
			{
				d.adwLook[k] = 0;
				if (row[6 + k])
					str_to_number(d.adwLook[k], row[6 + k]);
			}
			d.bJob = (BYTE)MIN(job, 7U);
			d.bLevel = (BYTE)MINMAX(1U, level, 120U);
			dancers.push_back(d);
			if (d.dwPid)
				pids.insert(d.dwPid);
			if ((int)dancers.size() >= PLAYERBOT_DANCE_MAX)
				break;
		}
		s_vecPlayerBotDancers.swap(dancers);
		s_setPlayerBotDancerPids.swap(pids);
	}

	void ReadPlayerBotDanceShow()
	{
		struct stat st;
		if (stat(PLAYERBOT_DANCE_SHOW_PATH, &st) != 0)
		{
			s_kPlayerBotDanceShow = TPlayerBotDanceShow();
			s_kPlayerBotDanceShow.dwOwner = 0;
			s_kPlayerBotDanceShow.iCount = 0;
			s_kPlayerBotDanceShow.iDance = 1;
			s_kPlayerBotDanceShow.dwRepeatMs = 0;
			s_kPlayerBotDanceShow.bQuiet = false;
			s_bPlayerBotDanceQuiet = false;
			s_tPlayerBotDanceShowMtime = 0;
			return;
		}
		if (st.st_mtime == s_tPlayerBotDanceShowMtime && !s_kPlayerBotDanceShow.strStamp.empty())
			return;
		s_tPlayerBotDanceShowMtime = st.st_mtime;
		FILE* fp = fopen(PLAYERBOT_DANCE_SHOW_PATH, "r");
		if (!fp)
			return;
		TPlayerBotDanceShow show;
		show.dwOwner = 0;
		show.iCount = 0;
		show.iDance = 1;
		show.dwRepeatMs = 0;
		show.strFormation = "row";
		show.bQuiet = false;
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			char key[64] = { 0 }, value[160] = { 0 };
			if (sscanf(line, "%63s %159s", key, value) < 2 || key[0] == '#')
				continue;
			if (!strcmp(key, "command"))
				show.strCommand = value;
			else if (!strcmp(key, "owner"))
				str_to_number(show.dwOwner, value);
			else if (!strcmp(key, "count"))
				str_to_number(show.iCount, value);
			else if (!strcmp(key, "formation"))
				show.strFormation = value;
			else if (!strcmp(key, "dance"))
				str_to_number(show.iDance, value);
			else if (!strcmp(key, "repeat"))
			{
				int seconds = 0;
				str_to_number(seconds, value);
				show.dwRepeatMs = (DWORD)MINMAX(0, seconds, 600) * 1000;
			}
			else if (!strcmp(key, "quiet"))
				show.bQuiet = value[0] == '1';
			else if (!strcmp(key, "stamp"))
				show.strStamp = value;
		}
		fclose(fp);
		show.iCount = MINMAX(0, show.iCount, PLAYERBOT_DANCE_MAX);
		show.iDance = MINMAX(1, show.iDance, 6);
		if (show.strStamp.empty())
			show.strStamp = "-";
		s_kPlayerBotDanceShow = show;
		s_bPlayerBotDanceQuiet = show.bQuiet;
		sys_log(0, "PLAYERBOT_DANCE: show command=%s owner=%u count=%d formation=%s dance=%d repeat_ms=%u stamp=%s",
				show.strCommand.c_str(), show.dwOwner, show.iCount, show.strFormation.c_str(), show.iDance,
				show.dwRepeatMs, show.strStamp.c_str());
	}

	// Where dancer i of n stands, beside the owner at (ox, oy).
	void GetPlayerBotDanceSpot(const std::string& formation, int i, int n, long ox, long oy, long& x, long& y)
	{
		const long s = PLAYERBOT_DANCE_SPACING;
		if (formation == "circle")
		{
			const double radius = std::max(400.0, (double)(n * s) / (2.0 * 3.14159265));
			const double angle = 2.0 * 3.14159265 * i / std::max(1, n);
			x = ox + (long)(radius * cos(angle));
			y = oy + (long)(radius * sin(angle));
			return;
		}
		int perRow = n, row = 0, col = i;
		if (formation == "rows")
		{
			perRow = (n + 1) / 2;
			row = i / std::max(1, perRow);
			col = i % std::max(1, perRow);
		}
		x = ox + (long)((col - (perRow - 1) / 2.0) * s);
		y = oy + 500 + row * 300;
	}

	// The same broadcast the client's /danceN makes (do_emotion), without the
	// emotion mask: the show is the operator's own.
	void SendPlayerBotDance(LPCHARACTER ch, int dance)
	{
		char chatbuf[64];
		int len = snprintf(chatbuf, sizeof(chatbuf), "dance%d %u %u", dance, (DWORD)ch->GetVID(), 0U);
		if (len < 0 || len >= (int)sizeof(chatbuf))
			len = sizeof(chatbuf) - 1;
		++len;
		TPacketGCChat pack_chat;
		memset(&pack_chat, 0, sizeof(pack_chat));
		pack_chat.header = HEADER_GC_CHAT;
		pack_chat.size = sizeof(TPacketGCChat) + len;
		pack_chat.type = CHAT_TYPE_COMMAND;
		pack_chat.id = 0;
		TEMP_BUFFER buf;
		buf.write(&pack_chat, sizeof(TPacketGCChat));
		buf.write(chatbuf, len);
		ch->PacketAround(buf.read_peek(), buf.size());
	}

	// A dancer that has never had an identity gets one in the owner's kingdom,
	// of its class: a seeded character that never played, renamed now.
	bool ClaimPlayerBotDancerIdentity(TPlayerBotDancer& d, BYTE empire)
	{
		if (!IsPlayerBotDancerNameValid(d.strName))
		{
			SetPlayerBotDancerStatus(d.dwRow, "blad: nick 2-24 znaki: litery, cyfry, [ ]");
			return false;
		}
		char query[2048];
		snprintf(query, sizeof(query),
				"SELECT 1 FROM player.player WHERE BINARY name=BINARY '%s' LIMIT 1", d.strName.c_str());
		{
			std::unique_ptr<SQLMsg> taken(AccountDB::instance().DirectQuery(query));
			if (taken.get() && taken->uiSQLErrno == 0 && taken->Get() && taken->Get()->uiNumRows > 0)
			{
				SetPlayerBotDancerStatus(d.dwRow, "blad: nick jest juz zajety");
				return false;
			}
		}
		snprintf(query, sizeof(query),
				"SELECT l.pid FROM common.playerbot_seed_state AS l "
				"JOIN player.player AS p ON p.id=l.pid "
				"JOIN account.account AS a ON a.id=p.account_id "
				"JOIN player.player_index AS pi ON pi.id=a.id "
				"WHERE l.seed_version=1 AND l.state IN ('complete','adopted') "
				"AND BINARY a.login=BINARY CONCAT('playerbot_',LPAD(l.pid-3,GREATEST(3,LENGTH(l.pid-3)),'0')) "
				"AND pi.pid1=l.pid AND pi.pid2=0 AND pi.pid3=0 AND pi.pid4=0 "
				"AND pi.empire=%u AND p.job=%u AND p.playtime=0 "
				"AND l.pid NOT IN (SELECT sidekick_pid FROM player.playerbot_sidekick) "
				"AND l.pid NOT IN (SELECT pid FROM player.playerbot_dancer WHERE pid IS NOT NULL) "
				"%s"
				"ORDER BY p.level, l.pid DESC LIMIT 60",
				(unsigned int)empire, (unsigned int)d.bJob,
				s_bPlayerBotSidekickPoolTable ? "AND l.pid NOT IN (SELECT pid FROM player.playerbot_sidekick_pool) " : "");
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (!msg.get() || msg->uiSQLErrno != 0 || !msg->Get() || !msg->Get()->pSQLResult)
		{
			SetPlayerBotDancerStatus(d.dwRow, "blad: zapytanie o postac");
			return false;
		}
		MYSQL_ROW row;
		while (NULL != (row = mysql_fetch_row(msg->Get()->pSQLResult)))
		{
			DWORD pid = 0;
			str_to_number(pid, row[0]);
			if (pid == 0 || !CPlayerBotManager::instance().IsRegisteredBotPID(pid) ||
					CPlayerBotManager::instance().IsManaged(pid) ||
					CPlayerBotManager::instance().IsMedalDropperCohortPID(pid) ||
					IsPlayerBotShouterPID(pid) || IsPlayerBotSidekickPID(pid) ||
					CHARACTER_MANAGER::instance().FindByPID(pid) || P2P_MANAGER::instance().FindByPID(pid))
				continue;
			// The pid first, so no other row and no population pass takes it,
			// then the name: a name the engine refuses leaves the row to retry.
			snprintf(query, sizeof(query), "UPDATE player.playerbot_dancer SET pid=%u WHERE id=%u AND pid IS NULL",
					pid, d.dwRow);
			if (PlayerBotDanceExec(query) != 1)
				return false;
			snprintf(query, sizeof(query), "UPDATE player.player SET name='%s' WHERE id=%u", d.strName.c_str(), pid);
			if (PlayerBotDanceExec(query) < 0)
			{
				snprintf(query, sizeof(query), "UPDATE player.playerbot_dancer SET pid=NULL WHERE id=%u", d.dwRow);
				PlayerBotDanceExec(query);
				SetPlayerBotDancerStatus(d.dwRow, "blad: nie udalo sie nadac nicku");
				return false;
			}
			d.dwPid = pid;
			s_setPlayerBotDancerPids.insert(pid);
			SetPlayerBotDancerStatus(d.dwRow, "postac przydzielona");
			sys_log(0, "PLAYERBOT_DANCE: identity pid=%u for row=%u name=%s job=%u empire=%u",
					pid, d.dwRow, d.strName.c_str(), (unsigned int)d.bJob, (unsigned int)empire);
			return true;
		}
		SetPlayerBotDancerStatus(d.dwRow, "blad: brak wolnej postaci tej klasy w krolestwie");
		return false;
	}

	// The name the panel gave a dancer that already has an identity, written
	// while it is out of the world long enough for the db core to have let the
	// old one go.
	void RenamePlayerBotDancerIfIdle(const TPlayerBotDancer& d)
	{
		if (!IsPlayerBotDancerNameValid(d.strName))
			return;
		char query[256];
		snprintf(query, sizeof(query),
				"UPDATE player.player SET name='%s' WHERE id=%u AND BINARY name<>BINARY '%s' "
				"AND (playtime=0 OR last_play < NOW() - INTERVAL %d MINUTE)",
				d.strName.c_str(), d.dwPid, d.strName.c_str(), PLAYERBOT_DANCE_RENAME_IDLE_MINUTES);
		if (PlayerBotDanceExec(query) > 0)
			sys_log(0, "PLAYERBOT_DANCE: renamed pid=%u to %s", d.dwPid, d.strName.c_str());
	}

	// The look the panel set for a dancer, put on as it is placed: each item is
	// made new and worn in the cell it is for, what was there goes. A piece it
	// already wears is left alone, so a second "Zbierz" makes nothing. The show
	// is the operator's: no level or class check (the panel warns of those).
	void ApplyPlayerBotDancerLook(LPCHARACTER ch, const TPlayerBotDancer& d)
	{
		bool changed = false;
		for (int k = 0; k < 5; ++k)
		{
			const DWORD vnum = d.adwLook[k];
			if (vnum == 0)
				continue;
			bool worn = false;
			for (int w = 0; w < WEAR_MAX_NUM && !worn; ++w)
			{
				LPITEM on = ch->GetWear(w);
				worn = on && on->GetVnum() == vnum;
			}
			if (worn)
				continue;
			LPITEM item = ITEM_MANAGER::instance().CreateItem(vnum, 1, 0, false);
			if (!item)
			{
				sys_err("PLAYERBOT_DANCE: no item vnum=%u for pid=%u", vnum, ch->GetPlayerID());
				continue;
			}
			const int cell = item->FindEquipCell(ch);
			LPITEM old = cell >= 0 ? ch->GetWear(cell) : NULL;
			if (cell < 0 || (old && IS_SET(old->GetFlag(), ITEM_FLAG_IRREMOVABLE)))
			{
				sys_err("PLAYERBOT_DANCE: cannot wear vnum=%u cell=%d pid=%u", vnum, cell, ch->GetPlayerID());
				M2_DESTROY_ITEM(item);
				continue;
			}
			if (old)
			{
				old->RemoveFromCharacter();
				ITEM_MANAGER::instance().RemoveItem(old, "PLAYERBOT_DANCE_LOOK");
			}
			if (!item->EquipTo(ch, (BYTE)cell))
			{
				sys_err("PLAYERBOT_DANCE: equip failed vnum=%u cell=%d pid=%u", vnum, cell, ch->GetPlayerID());
				M2_DESTROY_ITEM(item);
				continue;
			}
			changed = true;
			sys_log(0, "PLAYERBOT_DANCE: look pid=%u name=%s vnum=%u cell=%d", ch->GetPlayerID(), ch->GetName(), vnum, cell);
		}
		if (changed)
		{
			// The parts the others see (armour, weapon, hair) are worked out here.
			ch->ComputePoints();
			ch->UpdatePacket();
		}
	}

	void LogOutPlayerBotDancer(DWORD pid, const char* why)
	{
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
		if (ch)
		{
			if (ch->GetParty())
				LeavePlayerBotParty(ch);
			ch->Save();
		}
		sys_log(0, "PLAYERBOT_DANCE: logging out pid=%u why=%s", pid, why);
		CPlayerBotManager::instance().Despawn(pid, playerbot_session_rules::OUT_SIDEKICK);
		s_mapPlayerBotDancerPlaced.erase(pid);
	}

	// "Swiat bez botow": while the show asks for it, the population of this core
	// logs out, a few dozen a pass (each is a save), and the door stays shut
	// (IsPlayerBotSpawnHeld). The dancers and the players' companions stay.
	// When it is off again the population comes back through the usual spawn
	// window and the top-up, as after a restart.
	const int PLAYERBOT_DANCE_QUIET_BATCH = 40;
	int s_iPlayerBotDancePopulation = 0;

	void QuietPlayerBotWorld()
	{
		std::vector<DWORD> population;
		for (TPlayerBotAIStateMap::const_iterator it = s_mapPlayerBotAIStates.begin(); it != s_mapPlayerBotAIStates.end(); ++it)
		{
			const DWORD pid = it->first;
			if (IsPlayerBotDancerPID(pid) || IsPlayerBotSidekickPID(pid) || !CPlayerBotManager::instance().IsManaged(pid))
				continue;
			population.push_back(pid);
		}
		s_iPlayerBotDancePopulation = (int)population.size();
		if (!s_kPlayerBotDanceShow.bQuiet || population.empty())
			return;
		int done = 0;
		for (size_t i = 0; i < population.size() && done < PLAYERBOT_DANCE_QUIET_BATCH; ++i, ++done)
		{
			LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(population[i]);
			if (ch)
			{
				if (ch->GetParty())
					LeavePlayerBotParty(ch);
				ch->Save();
			}
			CPlayerBotManager::instance().Despawn(population[i], playerbot_session_rules::OUT_GM);
		}
		s_iPlayerBotDancePopulation -= done;
		PlayerBotLogThrottled("dance_quiet", get_dword_time(),
				"PLAYERBOT_DANCE: quiet world, logged out %d bots, %d left on this core", done, s_iPlayerBotDancePopulation);
	}

	void WritePlayerBotDanceStatus(DWORD dwNow, int present, int placed, bool ownerHere)
	{
		if (dwNow < s_dwPlayerBotDanceNextStatus)
			return;
		s_dwPlayerBotDanceNextStatus = dwNow + 1000;
		const TPlayerBotDanceShow& show = s_kPlayerBotDanceShow;
		// Only the core that holds the owner speaks for the show; another core
		// would overwrite it with "owner absent" every second.
		if (!ownerHere)
			return;
		char path[256];
		snprintf(path, sizeof(path), "%s.new.%u", PLAYERBOT_DANCE_STATUS_PATH, (unsigned int)g_bChannel);
		FILE* fp = fopen(path, "w");
		if (!fp)
			return;
		fprintf(fp, "time %ld\nchannel %u\nstamp %s\ncommand %s\nowner %u\nwanted %d\npresent %d\nplaced %d\n"
				"beats %u\nlast_beat %ld\nnote %s\nquiet %d\npopulation %d\n",
				(long)time(0), (unsigned int)g_bChannel, show.strStamp.c_str(), show.strCommand.c_str(), show.dwOwner,
				show.iCount, present, placed, s_dwPlayerBotDanceBeats, (long)s_tPlayerBotDanceLastBeat,
				s_strPlayerBotDanceNote.empty() ? "-" : s_strPlayerBotDanceNote.c_str(),
				show.bQuiet ? 1 : 0, s_iPlayerBotDancePopulation);
		fclose(fp);
		chmod(path, 0664);
		rename(path, PLAYERBOT_DANCE_STATUS_PATH);
	}

	void ManagePlayerBotDancers(DWORD dwNow)
	{
		// Four times a second is beat enough, and keeps a core without a show cheap.
		if (dwNow < s_dwPlayerBotDancerNextManage)
			return;
		s_dwPlayerBotDancerNextManage = dwNow + 250;
		if (dwNow >= s_dwPlayerBotDancerNextLoad)
		{
			s_dwPlayerBotDancerNextLoad = dwNow + PLAYERBOT_DANCE_RELOAD_MS;
			LoadPlayerBotDancers();
			ReadPlayerBotDanceShow();
		}
		const TPlayerBotDanceShow& show = s_kPlayerBotDanceShow;
		LPCHARACTER owner = show.dwOwner ? CHARACTER_MANAGER::instance().FindByPID(show.dwOwner) : NULL;
		if (owner && (!owner->GetDesc() || owner->GetDesc()->IsBot() || !owner->GetSectree()))
			owner = NULL;
		if (owner)
			s_dwPlayerBotDanceOwnerSeen = dwNow;
		const bool active = owner && (show.strCommand == "gather" || show.strCommand == "start");

		// Who should stand in the troupe now: the first `count` of the roster.
		std::set<DWORD> wanted;
		int present = 0, placed = 0;
		if (active)
		{
			int n = 0;
			const int total = std::min<int>(show.iCount, (int)s_vecPlayerBotDancers.size());
			for (size_t i = 0; i < s_vecPlayerBotDancers.size() && n < total; ++i, ++n)
			{
				TPlayerBotDancer& d = s_vecPlayerBotDancers[i];
				if (d.dwPid == 0)
				{
					// One claim every two seconds at most: each is a query over
					// the seed, and a row that cannot be served would ask again
					// on every pass.
					if (dwNow < s_dwPlayerBotDancerNextClaim)
						continue;
					s_dwPlayerBotDancerNextClaim = dwNow + 2000;
					if (!ClaimPlayerBotDancerIdentity(d, owner->GetEmpire()))
						continue;
				}
				wanted.insert(d.dwPid);
				const bool here = CPlayerBotManager::instance().IsManaged(d.dwPid);
				LPCHARACTER ch = here ? CHARACTER_MANAGER::instance().FindByPID(d.dwPid) : NULL;
				if (!here)
				{
					DWORD& next = s_mapPlayerBotDancerNextSpawn[d.dwPid];
					if (dwNow >= next && !P2P_MANAGER::instance().FindByPID(d.dwPid) &&
							!CHARACTER_MANAGER::instance().FindByPID(d.dwPid))
					{
						next = dwNow + PLAYERBOT_DANCE_SPAWN_RETRY_MS;
						RenamePlayerBotDancerIfIdle(d);
						const bool sent = CPlayerBotManager::instance().SpawnSidekick(d.dwPid);
						SetPlayerBotDancerStatus(d.dwRow, sent ? "loguje sie" : "blad: nie udalo sie zalogowac");
					}
					continue;
				}
				if (!ch || !ch->GetDesc() || !ch->GetDesc()->IsPhase(PHASE_GAME) || !ch->GetSectree())
					continue;
				++present;
				std::map<DWORD, std::string>::iterator mark = s_mapPlayerBotDancerPlaced.find(d.dwPid);
				if (mark != s_mapPlayerBotDancerPlaced.end() && mark->second == show.strStamp &&
						ch->GetMapIndex() == owner->GetMapIndex())
				{
					++placed;
					continue;
				}
				// The level the panel asked for (raised only - a level is not taken back).
				if ((int)ch->GetLevel() < (int)d.bLevel)
					RaisePlayerBotSidekickLevel(ch, d.bLevel);
				if (ch->IsDead())
					continue;
				ApplyPlayerBotDancerLook(ch, d);
				long x = 0, y = 0;
				GetPlayerBotDanceSpot(show.strFormation, n, total, owner->GetX(), owner->GetY(), x, y);
				PIXEL_POSITION pos;
				if (SECTREE_MANAGER::instance().GetMovablePosition(owner->GetMapIndex(), x, y, pos))
				{
					x = pos.x;
					y = pos.y;
				}
				TPlayerBotAIState& state = s_mapPlayerBotAIStates[d.dwPid];
				if (PlacePlayerBotSidekickAt(ch, state, owner->GetMapIndex(), x, y, dwNow, "dance_show"))
				{
					ch->SetRotation(owner->GetRotation());
					s_mapPlayerBotDancerPlaced[d.dwPid] = show.strStamp;
					SetPlayerBotDancerStatus(d.dwRow, "w szyku");
					++placed;
				}
			}
		}

		// Everybody of the troupe in this world who should not be: the show is
		// over, the owner left, the count went down or the row was deleted.
		const bool ownerGone = !owner && (s_dwPlayerBotDanceOwnerSeen == 0 ||
				dwNow - s_dwPlayerBotDanceOwnerSeen >= PLAYERBOT_DANCE_OWNER_GONE_MS);
		// A dancer this core never placed counts too (stopped while it loaded).
		// The owner a few seconds away (a warp between two maps) keeps them.
		// A row deleted in the panel is gone from the roster, not from this
		// world: what this core placed is asked too.
		std::set<DWORD> troupe = s_setPlayerBotDancerPids;
		for (std::map<DWORD, std::string>::const_iterator it = s_mapPlayerBotDancerPlaced.begin();
				it != s_mapPlayerBotDancerPlaced.end(); ++it)
			troupe.insert(it->first);
		for (std::set<DWORD>::const_iterator it = troupe.begin(); it != troupe.end(); ++it)
		{
			const DWORD pid = *it;
			if (!CPlayerBotManager::instance().IsManaged(pid))
			{
				s_mapPlayerBotDancerPlaced.erase(pid);
				continue;
			}
			if (active && wanted.count(pid))
				continue;
			if (!owner && !ownerGone)
				continue;
			LogOutPlayerBotDancer(pid, show.strCommand == "stop" ? "stop" : ownerGone ? "owner_gone" : "not_wanted");
		}

		// The beat: every placed dancer starts the dance in this one call.
		if (active && show.strCommand == "start")
		{
			if (s_strPlayerBotDanceBeatStamp != show.strStamp)
			{
				s_strPlayerBotDanceBeatStamp = show.strStamp;
				s_dwPlayerBotDanceNextBeat = dwNow + PLAYERBOT_DANCE_COUNTDOWN_MS;
				s_dwPlayerBotDanceBeats = 0;
			}
			if (s_dwPlayerBotDanceNextBeat != 0 && dwNow >= s_dwPlayerBotDanceNextBeat)
			{
				int danced = 0;
				for (std::set<DWORD>::const_iterator it = wanted.begin(); it != wanted.end(); ++it)
				{
					LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(*it);
					if (!ch || !ch->GetSectree() || ch->IsDead())
						continue;
					if (ch->IsStateMove())
						ch->Stop();
					SendPlayerBotDance(ch, show.iDance);
					++danced;
				}
				++s_dwPlayerBotDanceBeats;
				s_tPlayerBotDanceLastBeat = time(0);
				sys_log(0, "PLAYERBOT_DANCE: beat %u dance=%d dancers=%d", s_dwPlayerBotDanceBeats, show.iDance, danced);
				s_dwPlayerBotDanceNextBeat = show.dwRepeatMs ? s_dwPlayerBotDanceNextBeat + show.dwRepeatMs : 0;
				// A tick that came late does not fire a burst of beats to catch up.
				if (s_dwPlayerBotDanceNextBeat != 0 && s_dwPlayerBotDanceNextBeat <= dwNow)
					s_dwPlayerBotDanceNextBeat = dwNow + show.dwRepeatMs;
			}
		}
		else if (show.strCommand != "start")
		{
			s_strPlayerBotDanceBeatStamp.clear();
			s_dwPlayerBotDanceNextBeat = 0;
		}

		QuietPlayerBotWorld();
		s_strPlayerBotDanceNote = !show.dwOwner ? "no_owner" : !owner ? "owner_not_here" : "ok";
		WritePlayerBotDanceStatus(dwNow, present, placed, owner != NULL);
	}

	// The bot tick for a dancer: stand still, nothing else.
	void KeepPlayerBotDancer(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		if (ch->IsStateMove())
			ch->Stop();
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		state.dwLastMeaningfulActivityTime = dwNow;
	}
}
