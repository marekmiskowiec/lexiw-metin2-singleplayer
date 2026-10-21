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
//     formation row | rows | grid | column | wedge | vee | circle | halfcircle | manual
//     dance    1..6 (the client's dance1..dance6)
//     repeat   seconds between two starts of the dance, 0 = once
//     stamp    anything new each time the panel writes the file
// and, since the choreography (LEXIW_DANCE_SHOW_V2, 20 October):
//     anchor   owner (beside the owner, as before) | spot (a point of a map)
//     map, spot_x, spot_y  the spot: map index and local coordinates (cm)
//     facing   owner (towards the owner) | same (as the owner) | 0..359
//     transition walk | run | jump: how a formation step moves the troupe
//     countdown seconds before the first move; notice 1 = shown to the owner
//     marker   an emotion all of them make first, to cut the film to the song
//     daymode  keep | dark | light, snow keep | on | off (the owner's screen)
//     loop     seconds after which the script starts again, 0 = once
//     step     <ms> dance|emote|formation|pause <arg> <who> <every ms>
// With no step the show is one: step 0 dance <dance> all <repeat>.
// What this core sees of it goes back to the panel in
// /opt/m2spool/playerbot_dance.status. Only the core the owner stands on acts
// (with a spot: the core that hosts its map); the owner's own core writes
// where the owner stands to playerbot_dance.owner, for the panel's "here".
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
	// The countdown of a show file without one (before LEXIW_DANCE_SHOW_V2):
	// every dancer of a fresh gathering has its position before the music.
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
		// Its own place in the "manual" formation (cm): to the right, back.
		long lPosX;
		long lPosY;
	};

	struct TPlayerBotDanceStep
	{
		DWORD dwAt;
		std::string strAction;	// dance | emote | formation | pause
		std::string strArg;		// the emotion's command name, or the formation
		std::string strWho;		// all | odd | even | rowN | N | A-B (places from 1)
		DWORD dwEvery;			// again every so many ms until the next step for them
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
		bool bSpot;
		long lMap;
		long lSpotX;
		long lSpotY;
		std::string strFacing;
		std::string strTransition;
		DWORD dwCountdownMs;
		bool bNotice;
		std::string strMarker;
		std::string strDayMode;
		std::string strSnow;
		DWORD dwLoopMs;
		std::vector<TPlayerBotDanceStep> vecSteps;

		TPlayerBotDanceShow() : dwOwner(0), iCount(0), strFormation("row"), iDance(1), dwRepeatMs(0), bQuiet(false),
			bSpot(false), lMap(0), lSpotX(0), lSpotY(0), strFacing("owner"), strTransition("walk"),
			dwCountdownMs(PLAYERBOT_DANCE_COUNTDOWN_MS), bNotice(false), strDayMode("keep"), strSnow("keep"), dwLoopMs(0) {}
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
			// Its own place in the "manual" formation (LEXIW_DANCE_SHOW_V2).
			PlayerBotDanceExec("ALTER TABLE player.playerbot_dancer "
					"ADD COLUMN IF NOT EXISTS pos_x INT NOT NULL DEFAULT 0, "
					"ADD COLUMN IF NOT EXISTS pos_y INT NOT NULL DEFAULT 0");
		}
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(
				"SELECT id, slot, name, job, level, IFNULL(pid,0), weapon, armor, costume, hair, skin, pos_x, pos_y "
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
			d.lPosX = 0;
			d.lPosY = 0;
			if (row[11])
				str_to_number(d.lPosX, row[11]);
			if (row[12])
				str_to_number(d.lPosY, row[12]);
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
		char line[256];
		while (fgets(line, sizeof(line), fp))
		{
			char key[64] = { 0 }, value[160] = { 0 };
			if (sscanf(line, "%63s %159s", key, value) < 2 || key[0] == '#')
				continue;
			if (!strcmp(key, "step"))
			{
				char action[16] = { 0 }, arg[32] = { 0 }, who[32] = { 0 };
				unsigned int at = 0, every = 0;
				if (sscanf(line, "%*s %u %15s %31s %31s %u", &at, action, arg, who, &every) >= 4 &&
						show.vecSteps.size() < 200)
				{
					TPlayerBotDanceStep step;
					step.dwAt = MIN(at, 3600000U);
					step.strAction = action;
					step.strArg = arg;
					step.strWho = who;
					step.dwEvery = every ? MINMAX(500U, every, 600000U) : 0;
					show.vecSteps.push_back(step);
				}
				continue;
			}
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
			else if (!strcmp(key, "anchor"))
				show.bSpot = !strcmp(value, "spot");
			else if (!strcmp(key, "map"))
				str_to_number(show.lMap, value);
			else if (!strcmp(key, "spot_x"))
				str_to_number(show.lSpotX, value);
			else if (!strcmp(key, "spot_y"))
				str_to_number(show.lSpotY, value);
			else if (!strcmp(key, "facing"))
				show.strFacing = value;
			else if (!strcmp(key, "transition"))
				show.strTransition = value;
			else if (!strcmp(key, "countdown"))
			{
				int seconds = 0;
				str_to_number(seconds, value);
				show.dwCountdownMs = (DWORD)MINMAX(1, seconds, 30) * 1000;
			}
			else if (!strcmp(key, "notice"))
				show.bNotice = value[0] == '1';
			else if (!strcmp(key, "marker"))
				show.strMarker = strcmp(value, "-") ? value : "";
			else if (!strcmp(key, "daymode"))
				show.strDayMode = value;
			else if (!strcmp(key, "snow"))
				show.strSnow = value;
			else if (!strcmp(key, "loop"))
			{
				int seconds = 0;
				str_to_number(seconds, value);
				show.dwLoopMs = (DWORD)MINMAX(0, seconds, 3600) * 1000;
			}
		}
		fclose(fp);
		show.iCount = MINMAX(0, show.iCount, PLAYERBOT_DANCE_MAX);
		show.iDance = MINMAX(1, show.iDance, 24);
		if (show.strStamp.empty())
			show.strStamp = "-";
		if (show.bSpot && (show.lMap <= 0 || show.lSpotX < 0 || show.lSpotY < 0))
			show.bSpot = false;
		// The simple show (no script): one dance for all, again every `repeat`.
		if (show.vecSteps.empty())
		{
			TPlayerBotDanceStep step;
			step.dwAt = 0;
			step.strAction = "dance";
			char arg[16];
			snprintf(arg, sizeof(arg), "dance%d", show.iDance);
			step.strArg = arg;
			step.strWho = "all";
			step.dwEvery = show.dwRepeatMs;
			show.vecSteps.push_back(step);
		}
		std::stable_sort(show.vecSteps.begin(), show.vecSteps.end(),
				[](const TPlayerBotDanceStep& a, const TPlayerBotDanceStep& b) { return a.dwAt < b.dwAt; });
		s_kPlayerBotDanceShow = show;
		s_bPlayerBotDanceQuiet = show.bQuiet;
		sys_log(0, "PLAYERBOT_DANCE: show command=%s owner=%u count=%d formation=%s dance=%d repeat_ms=%u stamp=%s",
				show.strCommand.c_str(), show.dwOwner, show.iCount, show.strFormation.c_str(), show.iDance,
				show.dwRepeatMs, show.strStamp.c_str());
	}

	// Where the troupe stands: the point the formation is laid out from (the
	// front row's middle, or a circle's centre) and the way it faces, fixed
	// when a stamp is first acted on so the owner can walk off to film.
	struct TPlayerBotDanceAnchor
	{
		std::string strStamp;
		long lMap;
		long lX;
		long lY;
		float fFacing;
	};
	TPlayerBotDanceAnchor s_kPlayerBotDanceAnchor;
	std::string s_strPlayerBotDanceFormationNow;

	const double PLAYERBOT_DANCE_PI = 3.14159265358979;

	// Dancer i of n in a formation: (u to the right, v back from the front
	// row) in the troupe's own frame, the row it belongs to (for "rowN" in the
	// script) and, for a circle, the way it turns (-1 = the troupe's way).
	void GetPlayerBotDanceFrameSpot(const std::string& formation, int i, int n, const TPlayerBotDancer* d,
			double& u, double& v, int& row, double& turn)
	{
		const double s = PLAYERBOT_DANCE_SPACING;
		n = std::max(1, n);
		u = v = 0.0;
		row = 0;
		turn = -1.0;
		if (formation == "circle")
		{
			const double radius = std::max(400.0, n * s / (2.0 * PLAYERBOT_DANCE_PI));
			const double a = 2.0 * PLAYERBOT_DANCE_PI * i / n;
			u = radius * sin(a);
			v = -radius * cos(a);
			turn = a;	// towards the centre, worked out by the caller
			return;
		}
		if (formation == "halfcircle")
		{
			// An arc open to the audience: the ends forward, the middle back.
			const double radius = std::max(500.0, n * s / PLAYERBOT_DANCE_PI);
			const double end = PLAYERBOT_DANCE_PI * 0.42;
			const double a = n == 1 ? 0.0 : -end + 2.0 * end * i / (n - 1);
			u = radius * sin(a);
			v = radius * (cos(a) - cos(end));
			return;
		}
		if (formation == "column")
		{
			v = i * s;
			row = i;
			return;
		}
		if (formation == "wedge" || formation == "vee")
		{
			// The tip and then pairs, one step out and one back each: a wedge
			// points at the audience, a V opens to it.
			const int k = (i + 1) / 2;
			const int side = i % 2 ? -1 : 1;
			const int depth = (n) / 2;
			u = side * k * s * 0.8;
			v = (formation == "wedge" ? k : depth - k) * s * 0.8;
			row = formation == "wedge" ? k : depth - k;
			return;
		}
		if (formation == "manual" && d)
		{
			u = d->lPosX;
			v = d->lPosY;
			return;
		}
		int rows = formation == "rows" ? 2 : formation == "grid" ? 3 : 1;
		rows = std::min(rows, n);
		const int perRow = (n + rows - 1) / rows;
		row = i / perRow;
		const int col = i % perRow;
		const int inRow = std::min(perRow, n - row * perRow);
		// Behind each other the rows stand in the gaps, so every face shows.
		const double stagger = (row % 2 && inRow == perRow && perRow > 1) ? s * 0.5 : 0.0;
		u = (col - (inRow - 1) / 2.0) * s + stagger;
		v = row * 300.0;
	}

	// The world point and the rotation of dancer i of n around the anchor.
	void GetPlayerBotDanceSpot(const std::string& formation, int i, int n, const TPlayerBotDancer* d,
			const TPlayerBotDanceAnchor& anchor, long& x, long& y, float& rot, int& row)
	{
		double u = 0, v = 0, turn = -1;
		GetPlayerBotDanceFrameSpot(formation, i, n, d, u, v, row, turn);
		// The troupe faces fFacing: forward (sin, cos) in the engine's degrees,
		// right (cos, -sin); the rows go back, away from where it faces.
		const double f = anchor.fFacing * PLAYERBOT_DANCE_PI / 180.0;
		const double fx = sin(f), fy = cos(f), rx = cos(f), ry = -sin(f);
		x = anchor.lX + (long)(u * rx - v * fx);
		y = anchor.lY + (long)(u * ry - v * fy);
		rot = anchor.fFacing;
		if (turn >= 0.0)
			rot = (float)GetDegreeFromPositionXY(x, y, anchor.lX, anchor.lY);
	}

	// The same broadcast the client's emotions make (do_emotion: "danceN",
	// "clap", ...), without the emotion mask: the show is the operator's own.
	// LEXIW_KAIA_DANCES_V1: dance7..dance24 are the custom dances of the
	// patched client (the "Dances by KaiaProduction" pack): its exe knows only
	// dance1..6, so they go as "kaia_dance <vid> <n>", which its game.py plays.
	// 25 and 26 are the backflip and the push-up of the new MT2009 client,
	// in the same patched root.
	int GetPlayerBotKaiaDance(const std::string& name)
	{
		if (name == "backflip")
			return 25;
		if (name == "pushup")
			return 26;
		int n = 0;
		if (name.compare(0, 5, "dance") != 0 || name.size() < 6 || name.size() > 7)
			return 0;
		str_to_number(n, name.c_str() + 5);
		return n >= 7 && n <= 24 ? n : 0;
	}

	bool IsPlayerBotDanceEmotion(const std::string& name)
	{
		static const char* const names[] = { "dance1", "dance2", "dance3", "dance4", "dance5", "dance6",
			"clap", "cheer1", "cheer2", "congratulation", "forgive", "angry", "attractive", "sad", "shy",
			"cheerup", "banter", "joy", NULL };
		for (int i = 0; names[i]; ++i)
			if (name == names[i])
				return true;
		return GetPlayerBotKaiaDance(name) != 0;
	}

	// A client command ("danceN <vid> 0", "kaia_dance <vid> <n>", ...) to
	// everybody who sees the dancer.
	void SendPlayerBotDanceCommand(LPCHARACTER ch, const char* chatbuf, int len)
	{
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

	void SendPlayerBotEmotion(LPCHARACTER ch, const std::string& emotion)
	{
		if (!IsPlayerBotDanceEmotion(emotion))
			return;
		char chatbuf[64];
		const int kaia = GetPlayerBotKaiaDance(emotion);
		int len = kaia ? snprintf(chatbuf, sizeof(chatbuf), "kaia_dance %u %d", (DWORD)ch->GetVID(), kaia)
			: snprintf(chatbuf, sizeof(chatbuf), "%s %u %u", emotion.c_str(), (DWORD)ch->GetVID(), 0U);
		if (len < 0 || len >= (int)sizeof(chatbuf))
			len = sizeof(chatbuf) - 1;
		SendPlayerBotDanceCommand(ch, chatbuf, len + 1);
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

	void ClearPlayerBotDancerLook(LPCHARACTER ch);

	void LogOutPlayerBotDancer(DWORD pid, const char* why)
	{
		LPCHARACTER ch = CHARACTER_MANAGER::instance().FindByPID(pid);
		if (ch)
		{
			if (ch->GetParty())
				LeavePlayerBotParty(ch);
			// The show's auras and monster shape are not kept (LEXIW_DANCE_EFFECTS_V1).
			ClearPlayerBotDancerLook(ch);
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

	// The show in time (LEXIW_DANCE_SHOW_V2): after "start" a countdown, the
	// marker emotion, then the script's steps from its own zero, again after
	// `loop`. An emotion step with `every` keeps going for its dancers until
	// a later step speaks for them.
	struct TPlayerBotDanceLoop
	{
		std::string strEmotion;
		DWORD dwEvery;
		DWORD dwNext;
	};
	struct TPlayerBotDanceWalk
	{
		long lX;
		long lY;
		float fRot;
		DWORD dwSince;
	};
	const DWORD PLAYERBOT_DANCE_MARKER_MS = 3000;
	const DWORD PLAYERBOT_DANCE_WALK_GIVE_UP_MS = 20000;
	std::map<DWORD, TPlayerBotDanceLoop> s_mapPlayerBotDanceLoops;
	std::map<DWORD, TPlayerBotDanceWalk> s_mapPlayerBotDancerWalks;
	DWORD s_dwPlayerBotDanceStartAt = 0;
	DWORD s_dwPlayerBotDanceScriptAt = 0;
	int s_iPlayerBotDanceNoticeLeft = 0;
	bool s_bPlayerBotDanceMarkerDone = false;
	size_t s_nPlayerBotDanceNextStep = 0;
	std::string s_strPlayerBotDancePhase = "-";
	// The anchor's settings and the round of placing: a "start" after a
	// "gather" with the same settings keeps the troupe where it stands.
	std::string s_strPlayerBotDanceAnchorKey;
	DWORD s_dwPlayerBotDancePlacing = 0;
	// The owner's sky: what the show set, to give back when it ends.
	bool s_bPlayerBotDanceSkySet = false;
	long s_lPlayerBotDanceSkyMap = 0;
	std::string s_strPlayerBotDanceSkyStamp;
	DWORD s_dwPlayerBotDanceOwnerEmpirePid = 0;
	BYTE s_bPlayerBotDanceOwnerEmpire = 0;
	DWORD s_dwPlayerBotDanceNextOwnerNote = 0;

	bool IsPlayerBotDanceFormation(const std::string& f)
	{
		return f == "row" || f == "rows" || f == "grid" || f == "column" || f == "wedge" || f == "vee" ||
			f == "circle" || f == "halfcircle" || f == "manual";
	}

	// The owner's kingdom, for a dancer's identity, with the owner on another
	// core of the channel (a spot on this core's map).
	BYTE GetPlayerBotDanceOwnerEmpire(LPCHARACTER owner, DWORD ownerPid)
	{
		if (owner)
			return owner->GetEmpire();
		if (s_dwPlayerBotDanceOwnerEmpirePid == ownerPid)
			return s_bPlayerBotDanceOwnerEmpire;
		s_dwPlayerBotDanceOwnerEmpirePid = ownerPid;
		s_bPlayerBotDanceOwnerEmpire = 0;
		char query[256];
		snprintf(query, sizeof(query), "SELECT pi.empire FROM player.player AS p "
				"JOIN player.player_index AS pi ON pi.id=p.account_id WHERE p.id=%u", ownerPid);
		std::unique_ptr<SQLMsg> msg(AccountDB::instance().DirectQuery(query));
		if (msg.get() && msg->uiSQLErrno == 0 && msg->Get() && msg->Get()->pSQLResult)
		{
			MYSQL_ROW row = mysql_fetch_row(msg->Get()->pSQLResult);
			if (row && row[0])
				str_to_number(s_bPlayerBotDanceOwnerEmpire, row[0]);
		}
		return s_bPlayerBotDanceOwnerEmpire;
	}

	// Fixes the anchor for this show; false while it cannot be (the owner is
	// not here, the spot's map is not on this core).
	bool PreparePlayerBotDanceAnchor(const TPlayerBotDanceShow& show, LPCHARACTER owner)
	{
		char key[256];
		snprintf(key, sizeof(key), "%u|%d|%ld|%ld|%ld|%s|%s", show.dwOwner, show.bSpot ? 1 : 0, show.lMap, show.lSpotX,
				show.lSpotY, show.strFacing.c_str(), show.strFormation.c_str());
		if (s_kPlayerBotDanceAnchor.strStamp == show.strStamp)
			return true;
		// "start" after "gather" (or a second "start") with the same settings:
		// the troupe stays where it was put, the owner may have walked off to film.
		if (show.strCommand == "start" && !s_kPlayerBotDanceAnchor.strStamp.empty() && s_strPlayerBotDanceAnchorKey == key)
		{
			s_kPlayerBotDanceAnchor.strStamp = show.strStamp;
			return true;
		}
		TPlayerBotDanceAnchor a;
		a.strStamp = show.strStamp;
		int degrees = -1;
		if (show.strFacing != "owner" && show.strFacing != "same")
		{
			str_to_number(degrees, show.strFacing.c_str());
			degrees = ((degrees % 360) + 360) % 360;
		}
		if (show.bSpot)
		{
			PIXEL_POSITION base;
			if (!SECTREE_MANAGER::instance().GetMap(show.lMap) ||
					!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(show.lMap, base))
				return false;
			a.lMap = show.lMap;
			a.lX = base.x + show.lSpotX;
			a.lY = base.y + show.lSpotY;
			const bool ownerBy = owner && owner->GetMapIndex() == a.lMap;
			if (degrees >= 0)
				a.fFacing = (float)degrees;
			else if (show.strFacing == "same" && ownerBy)
				a.fFacing = owner->GetRotation();
			else if (ownerBy && (owner->GetX() != a.lX || owner->GetY() != a.lY))
				a.fFacing = GetDegreeFromPositionXY(a.lX, a.lY, owner->GetX(), owner->GetY());
			else
				a.fFacing = 180.0f;
		}
		else
		{
			if (!owner)
				return false;
			const float r = owner->GetRotation();
			const double rad = r * PLAYERBOT_DANCE_PI / 180.0;
			a.lMap = owner->GetMapIndex();
			// In front of the owner, or round the owner for a circle.
			const long ahead = show.strFormation == "circle" ? 0 : 600;
			a.lX = owner->GetX() + (long)(sin(rad) * ahead);
			a.lY = owner->GetY() + (long)(cos(rad) * ahead);
			a.fFacing = degrees >= 0 ? (float)degrees : show.strFacing == "same" ? r : (float)fmod(r + 180.0f, 360.0f);
		}
		s_kPlayerBotDanceAnchor = a;
		s_strPlayerBotDanceAnchorKey = key;
		s_strPlayerBotDanceFormationNow = show.strFormation;
		++s_dwPlayerBotDancePlacing;
		s_mapPlayerBotDancerWalks.clear();
		sys_log(0, "PLAYERBOT_DANCE: anchor map=%ld x=%ld y=%ld facing=%.0f formation=%s stamp=%s",
				a.lMap, a.lX, a.lY, a.fFacing, show.strFormation.c_str(), show.strStamp.c_str());
		return true;
	}

	void ClearPlayerBotDanceAnchor()
	{
		s_kPlayerBotDanceAnchor = TPlayerBotDanceAnchor();
		s_strPlayerBotDanceAnchorKey.clear();
		s_mapPlayerBotDancerWalks.clear();
	}

	// "rowN", "odd", "1-5", "3", "all": does it name place i (from 0)?
	bool IsPlayerBotDanceWho(const std::string& who, int i, int row)
	{
		if (who.empty() || who == "all" || who == "-")
			return true;
		if (who == "odd")
			return (i + 1) % 2 == 1;
		if (who == "even")
			return (i + 1) % 2 == 0;
		if (who.compare(0, 3, "row") == 0)
		{
			int n = 0;
			str_to_number(n, who.c_str() + 3);
			return row + 1 == n;
		}
		int a = 0, b = 0;
		if (sscanf(who.c_str(), "%d-%d", &a, &b) == 2)
			return i + 1 >= a && i + 1 <= b;
		str_to_number(a, who.c_str());
		return i + 1 == a;
	}

	// Puts a dancer on its spot at once, turned the troupe's way.
	bool JumpPlayerBotDancer(LPCHARACTER ch, long x, long y, float rot, DWORD dwNow)
	{
		TPlayerBotAIState& state = s_mapPlayerBotAIStates[ch->GetPlayerID()];
		s_mapPlayerBotDancerWalks.erase(ch->GetPlayerID());
		if (!PlacePlayerBotSidekickAt(ch, state, s_kPlayerBotDanceAnchor.lMap, x, y, dwNow, "dance_show"))
			return false;
		ch->SetNowWalking(false);
		ch->SetRotation(rot);
		ch->SendMovePacket(FUNC_MOVE, 0, x, y, 0, dwNow);
		return true;
	}

	// Where place i of the troupe stands in the formation now.
	void GetPlayerBotDancerSpotNow(int i, int n, const TPlayerBotDancer* d, long& x, long& y, float& rot, int& row)
	{
		GetPlayerBotDanceSpot(s_strPlayerBotDanceFormationNow, i, n, d, s_kPlayerBotDanceAnchor, x, y, rot, row);
		PIXEL_POSITION pos;
		if (SECTREE_MANAGER::instance().GetMovablePosition(s_kPlayerBotDanceAnchor.lMap, x, y, pos))
		{
			x = pos.x;
			y = pos.y;
		}
	}

	// A formation step: everybody of the troupe goes to the new spot, walking,
	// running or at once ("transition").
	void MovePlayerBotDanceTroupe(const std::vector<DWORD>& order, const std::vector<const TPlayerBotDancer*>& rows,
			const std::string& transition, DWORD dwNow)
	{
		const int n = (int)order.size();
		for (int i = 0; i < n; ++i)
		{
			LPCHARACTER ch = order[i] ? CHARACTER_MANAGER::instance().FindByPID(order[i]) : NULL;
			if (!ch || !ch->GetSectree() || ch->IsDead())
				continue;
			long x = 0, y = 0;
			float rot = 0;
			int row = 0;
			GetPlayerBotDancerSpotNow(i, n, rows[i], x, y, rot, row);
			if (transition == "jump" || ch->GetMapIndex() != s_kPlayerBotDanceAnchor.lMap)
			{
				JumpPlayerBotDancer(ch, x, y, rot, dwNow);
				continue;
			}
			s_mapPlayerBotDanceLoops.erase(order[i]);
			TPlayerBotDanceWalk walk;
			walk.lX = x;
			walk.lY = y;
			walk.fRot = rot;
			walk.dwSince = dwNow;
			s_mapPlayerBotDancerWalks[order[i]] = walk;
			ch->SetNowWalking(transition == "walk");
			ch->SetRotationToXY(x, y);
			if (ch->Goto(x, y))
				ch->SendMovePacket(FUNC_MOVE, 0, x, y, ch->GetCurrentMoveDuration(), dwNow);
		}
	}

	DWORD SendPlayerBotDanceEmotion(LPCHARACTER ch, const std::string& emotion)
	{
		if (!ch || !ch->GetSectree() || ch->IsDead() ||
				s_mapPlayerBotDancerWalks.find(ch->GetPlayerID()) != s_mapPlayerBotDancerWalks.end())
			return 0;
		if (ch->IsStateMove())
			ch->Stop();
		SendPlayerBotEmotion(ch, emotion);
		return 1;
	}

	// LEXIW_DANCE_EFFECTS_V1: the show's effects. An "effect" is one burst over
	// a dancer (the engine's special effects or a client effect file); an
	// "aura" an affect whose flag the client draws (a skill's glow), on its
	// own affect type so several can be on at once and all go at the end; a
	// "poly" turns the dancer into a monster until "unpoly".
	struct TPlayerBotDanceEffect
	{
		const char* szName;
		int iSpecial;			// SE_*, or -1
		const char* szFile;		// a client .mse, or NULL
	};
	const TPlayerBotDanceEffect PLAYERBOT_DANCE_EFFECTS[] = {
		{ "firework", SE_CHINA_FIREWORK, NULL },
		{ "spintop", SE_SPIN_TOP, NULL },
		{ "success", SE_SUCCESS, NULL },
		{ "critical", SE_CRITICAL, NULL },
		{ "penetrate", SE_PENETRATE, NULL },
		{ "hpup", SE_HPUP_RED, NULL },
		{ "spup", SE_SPUP_BLUE, NULL },
		{ "speedup", SE_SPEEDUP_GREEN, NULL },
		{ "dxup", SE_DXUP_PURPLE, NULL },
		{ "cape", SE_BRAVERY_CAPE, NULL },
		{ "flameblow", SE_FLAME_BLOW, NULL },
		{ "firecracker", -1, "d:/ymir work/effect/etc/firecracker/firecracker_xmas.mse" },
		// Over the head: the client's emoticons, in slots the patched game.py
		// registers ("kaia_emote <vid> <name>", tools/kaia-dances).
		{ "heart", -1, "emo:" },
		{ "happy", -1, "emo:" },
		{ "celebration", -1, "emo:" },
		{ "angry", -1, "emo:" },
		{ "aha", -1, "emo:" },
		{ "question", -1, "emo:" },
		{ "exclaim", -1, "emo:" },
		{ "sweat", -1, "emo:" },
		{ "money", -1, "emo:" },
		{ "siren", -1, "emo:" },
		{ "whirl", -1, "emo:" },
		{ "doze", -1, "emo:" },
		{ "alcohol", -1, "emo:" },
		{ "letter", -1, "emo:" },
		{ "gloom", -1, "emo:" },
		{ "hearts", -1, "emo:" },
		{ "levelup", -1, "d:/ymir work/effect/etc/levelup_1/level_up.mse" },
		{ "levelup2", -1, "d:/ymir work/effect/etc/levelup_2/2.mse" },
		{ "start", -1, "d:/ymir work/effect/etc/compete/start.mse" },
		{ "snowflake", -1, "d:/ymir work/effect/etc/buff/buff_snowflower.mse" },
		{ "flash", -1, "d:/ymir work/effect/hit/gyeonggong_boom.mse" },
		{ "lightning", -1, "d:/ymir work/effect/hit/blow_electric/light_3_body.mse" },
		{ "fire_red", -1, "d:/ymir work/effect/background/moonlight_fire_red.mse" },
		{ "fire_blue", -1, "d:/ymir work/effect/background/moonlight_fire_blue.mse" },
		{ "fire_green", -1, "d:/ymir work/effect/background/moonlight_fire_green.mse" },
		{ "fire_violet", -1, "d:/ymir work/effect/background/moonlight_fire_violet.mse" },
		{ "fire_yellow", -1, "d:/ymir work/effect/background/moonlight_fire_yellow.mse" },
		{ "bats", -1, "d:/ymir work/effect/background/moonlight_eff_bat.mse" },
		{ "smoke", -1, "d:/ymir work/effect/background/moonlight_smoke_01.mse" },
		{ NULL, -1, NULL }
	};

	struct TPlayerBotDanceAura
	{
		const char* szName;
		DWORD dwFlag;
	};
	const TPlayerBotDanceAura PLAYERBOT_DANCE_AURAS[] = {
		{ "sword", AFF_SKILL_SWORD_AURA },
		{ "berserk", AFF_SKILL_BERSERK },
		{ "strongbody", AFF_SKILL_STRONG_BODY },
		{ "blade", AFF_SKILL_ENCHANTED_BLADE },
		{ "terror", AFF_SKILL_TERROR },
		{ "armor", AFF_SKILL_ENCHANTED_ARMOR },
		{ "blessing", AFF_SKILL_BLESSING },
		{ "reflect", AFF_SKILL_REFLECT },
		{ "swiftness", AFF_SKILL_SWIFTNESS },
		{ "manashield", AFF_SKILL_MANASHIELD },
		{ "flamespirit", AFF_SKILL_FLAME_SPIRIT },
		{ "dragon", AFF_SKILL_DRAGON_HELP },
		{ "attackup", AFF_SKILL_ATTACK_UP },
		{ "feather", AFF_SKILL_FEATHER_WALK },
		{ "fire", AFF_FIRE },
		{ "poison", AFF_POISON },
		{ NULL, 0 }
	};
	// The aura i is affect type PLAYERBOT_DANCE_AFFECT_BASE + i (free in
	// EAffectTypes, which ends in the 500s).
	const DWORD PLAYERBOT_DANCE_AFFECT_BASE = 960;

	DWORD PlayPlayerBotDanceEffect(LPCHARACTER ch, const std::string& name)
	{
		if (!ch || !ch->GetSectree() || ch->IsDead())
			return 0;
		for (int i = 0; PLAYERBOT_DANCE_EFFECTS[i].szName; ++i)
		{
			const TPlayerBotDanceEffect& e = PLAYERBOT_DANCE_EFFECTS[i];
			if (name != e.szName)
				continue;
			if (e.iSpecial >= 0)
				ch->EffectPacket(e.iSpecial);
			else if (!strcmp(e.szFile, "emo:"))
			{
				char chatbuf[64];
				int len = snprintf(chatbuf, sizeof(chatbuf), "kaia_emote %u %s", (DWORD)ch->GetVID(), e.szName);
				if (len < 0 || len >= (int)sizeof(chatbuf))
					len = sizeof(chatbuf) - 1;
				SendPlayerBotDanceCommand(ch, chatbuf, len + 1);
			}
			else
			{
				char file[MAX_EFFECT_FILE_NAME];
				memset(file, 0, sizeof(file));
				strlcpy(file, e.szFile, sizeof(file));
				ch->SpecificEffectPacket(file);
			}
			return 1;
		}
		return 0;
	}

	void ClearPlayerBotDancerLook(LPCHARACTER ch)
	{
		for (int i = 0; PLAYERBOT_DANCE_AURAS[i].szName; ++i)
			if (ch->FindAffect(PLAYERBOT_DANCE_AFFECT_BASE + i))
				ch->RemoveAffect(PLAYERBOT_DANCE_AFFECT_BASE + i);
		if (ch->IsPolymorphed())
			ch->SetPolymorph(0);
	}

	void SetPlayerBotDancerLook(LPCHARACTER ch, const std::string& action, const std::string& arg, DWORD dwMs)
	{
		if (action == "poly")
		{
			DWORD vnum = 0;
			str_to_number(vnum, arg.c_str());
			if (vnum && CMobManager::instance().Get(vnum))
			{
				if (ch->IsStateMove())
					ch->Stop();
				ch->SetPolymorph(vnum, true);
			}
			return;
		}
		if (action == "unpoly")
		{
			if (ch->IsPolymorphed())
				ch->SetPolymorph(0);
			return;
		}
		for (int i = 0; PLAYERBOT_DANCE_AURAS[i].szName; ++i)
		{
			const bool all = action == "noaura" && (arg.empty() || arg == "-");
			if (!all && arg != PLAYERBOT_DANCE_AURAS[i].szName)
				continue;
			const DWORD type = PLAYERBOT_DANCE_AFFECT_BASE + i;
			if (action == "noaura")
			{
				if (ch->FindAffect(type))
					ch->RemoveAffect(type);
				continue;
			}
			const long seconds = MINMAX(1L, (long)(dwMs ? dwMs / 1000 : 30), 3600L);
			ch->AddAffect(type, POINT_NONE, 0, PLAYERBOT_DANCE_AURAS[i].dwFlag, seconds, 0, true);
			return;
		}
	}

	void RunPlayerBotDanceStep(const TPlayerBotDanceStep& step, const std::vector<DWORD>& order,
			const std::vector<const TPlayerBotDancer*>& rows, const TPlayerBotDanceShow& show, DWORD dwNow)
	{
		const int n = (int)order.size();
		if (step.strAction == "formation")
		{
			if (!IsPlayerBotDanceFormation(step.strArg))
				return;
			s_strPlayerBotDanceFormationNow = step.strArg;
			MovePlayerBotDanceTroupe(order, rows, show.strTransition, dwNow);
			sys_log(0, "PLAYERBOT_DANCE: step at=%u formation=%s transition=%s", step.dwAt, step.strArg.c_str(),
					show.strTransition.c_str());
			return;
		}
		int done = 0;
		for (int i = 0; i < n; ++i)
		{
			if (!order[i])
				continue;
			long x = 0, y = 0;
			float rot = 0;
			int row = 0;
			GetPlayerBotDanceSpot(s_strPlayerBotDanceFormationNow, i, n, rows[i], s_kPlayerBotDanceAnchor, x, y, rot, row);
			if (!IsPlayerBotDanceWho(step.strWho, i, row))
				continue;
			if (step.strAction == "pause")
			{
				s_mapPlayerBotDanceLoops.erase(order[i]);
				continue;
			}
			LPCHARACTER who = CHARACTER_MANAGER::instance().FindByPID(order[i]);
			if (step.strAction == "aura" || step.strAction == "noaura" || step.strAction == "poly" ||
					step.strAction == "unpoly")
			{
				if (who && who->GetSectree() && !who->IsDead())
				{
					SetPlayerBotDancerLook(who, step.strAction, step.strArg, step.dwEvery);
					++done;
				}
				continue;
			}
			const bool effect = step.strAction == "effect";
			const std::string loopName = effect ? "fx:" + step.strArg : step.strArg;
			done += effect ? PlayPlayerBotDanceEffect(who, step.strArg) : SendPlayerBotDanceEmotion(who, step.strArg);
			if (step.dwEvery)
			{
				TPlayerBotDanceLoop& loop = s_mapPlayerBotDanceLoops[order[i]];
				loop.strEmotion = loopName;
				loop.dwEvery = step.dwEvery;
				loop.dwNext = dwNow + step.dwEvery;
			}
			else
				s_mapPlayerBotDanceLoops.erase(order[i]);
		}
		if (done)
		{
			++s_dwPlayerBotDanceBeats;
			s_tPlayerBotDanceLastBeat = time(0);
		}
		sys_log(0, "PLAYERBOT_DANCE: step at=%u %s %s who=%s every=%u dancers=%d", step.dwAt, step.strAction.c_str(),
				step.strArg.c_str(), step.strWho.c_str(), step.dwEvery, done);
	}

	void RunPlayerBotDanceTimeline(const TPlayerBotDanceShow& show, LPCHARACTER owner, const std::vector<DWORD>& order,
			const std::vector<const TPlayerBotDancer*>& rows, DWORD dwNow)
	{
		if (s_strPlayerBotDanceBeatStamp != show.strStamp)
		{
			s_strPlayerBotDanceBeatStamp = show.strStamp;
			s_dwPlayerBotDanceStartAt = dwNow + show.dwCountdownMs;
			s_iPlayerBotDanceNoticeLeft = (int)(show.dwCountdownMs / 1000);
			s_bPlayerBotDanceMarkerDone = show.strMarker.empty();
			s_dwPlayerBotDanceScriptAt = s_dwPlayerBotDanceStartAt + (show.strMarker.empty() ? 0 : PLAYERBOT_DANCE_MARKER_MS);
			s_nPlayerBotDanceNextStep = 0;
			s_dwPlayerBotDanceBeats = 0;
			s_mapPlayerBotDanceLoops.clear();
			// Every take starts plain: no aura or monster shape from the last one.
			for (size_t i = 0; i < order.size(); ++i)
			{
				LPCHARACTER ch = order[i] ? CHARACTER_MANAGER::instance().FindByPID(order[i]) : NULL;
				if (ch)
					ClearPlayerBotDancerLook(ch);
			}
			// A script that changed the formation: back to the starting one.
			if (s_strPlayerBotDanceFormationNow != show.strFormation)
			{
				s_strPlayerBotDanceFormationNow = show.strFormation;
				MovePlayerBotDanceTroupe(order, rows, "jump", dwNow);
			}
		}
		// The countdown, on the owner's screen: 3, 2, 1.
		while (s_iPlayerBotDanceNoticeLeft > 0 &&
				dwNow + (DWORD)s_iPlayerBotDanceNoticeLeft * 1000 >= s_dwPlayerBotDanceStartAt)
		{
			if (show.bNotice && owner)
				owner->ChatPacket(CHAT_TYPE_BIG_NOTICE, "%d", s_iPlayerBotDanceNoticeLeft);
			--s_iPlayerBotDanceNoticeLeft;
		}
		if (dwNow < s_dwPlayerBotDanceStartAt)
		{
			s_strPlayerBotDancePhase = "countdown";
			return;
		}
		if (!s_bPlayerBotDanceMarkerDone)
		{
			s_bPlayerBotDanceMarkerDone = true;
			int done = 0;
			for (size_t i = 0; i < order.size(); ++i)
				done += order[i] ? SendPlayerBotDanceEmotion(CHARACTER_MANAGER::instance().FindByPID(order[i]), show.strMarker) : 0;
			sys_log(0, "PLAYERBOT_DANCE: marker %s dancers=%d", show.strMarker.c_str(), done);
		}
		if (dwNow < s_dwPlayerBotDanceScriptAt)
		{
			s_strPlayerBotDancePhase = "marker";
			return;
		}
		while (s_nPlayerBotDanceNextStep < show.vecSteps.size() &&
				dwNow >= s_dwPlayerBotDanceScriptAt + show.vecSteps[s_nPlayerBotDanceNextStep].dwAt)
			RunPlayerBotDanceStep(show.vecSteps[s_nPlayerBotDanceNextStep++], order, rows, show, dwNow);
		if (s_nPlayerBotDanceNextStep >= show.vecSteps.size() && show.dwLoopMs &&
				dwNow >= s_dwPlayerBotDanceScriptAt + show.dwLoopMs)
		{
			s_dwPlayerBotDanceScriptAt += show.dwLoopMs;
			// A tick that came late does not run a burst of rounds to catch up.
			if (s_dwPlayerBotDanceScriptAt + show.dwLoopMs <= dwNow)
				s_dwPlayerBotDanceScriptAt = dwNow;
			s_nPlayerBotDanceNextStep = 0;
		}
		// The emotions that keep going.
		int looped = 0;
		for (std::map<DWORD, TPlayerBotDanceLoop>::iterator it = s_mapPlayerBotDanceLoops.begin();
				it != s_mapPlayerBotDanceLoops.end(); ++it)
		{
			TPlayerBotDanceLoop& loop = it->second;
			if (dwNow < loop.dwNext)
				continue;
			LPCHARACTER looper = CHARACTER_MANAGER::instance().FindByPID(it->first);
			looped += loop.strEmotion.compare(0, 3, "fx:") == 0 ? PlayPlayerBotDanceEffect(looper, loop.strEmotion.substr(3))
				: SendPlayerBotDanceEmotion(looper, loop.strEmotion);
			loop.dwNext += loop.dwEvery;
			if (loop.dwNext <= dwNow)
				loop.dwNext = dwNow + loop.dwEvery;
		}
		if (looped)
		{
			++s_dwPlayerBotDanceBeats;
			s_tPlayerBotDanceLastBeat = time(0);
		}
		s_strPlayerBotDancePhase = s_nPlayerBotDanceNextStep < show.vecSteps.size() || show.dwLoopMs ||
			!s_mapPlayerBotDanceLoops.empty() ? "dancing" : "done";
	}

	void StopPlayerBotDanceTimeline()
	{
		s_strPlayerBotDanceBeatStamp.clear();
		s_mapPlayerBotDanceLoops.clear();
		s_dwPlayerBotDanceStartAt = 0;
		s_strPlayerBotDancePhase = "-";
	}

	// Night or day and snow on the owner's screen for the show (the client's
	// "DayMode" and "xmas_snow"), given back from the world's event flags
	// when it ends. The owner's map change sends it again.
	void SetPlayerBotDanceSky(const TPlayerBotDanceShow& show, LPCHARACTER owner, bool active)
	{
		if (!owner)
			return;
		const bool wants = active && (show.strDayMode == "dark" || show.strDayMode == "light" ||
				show.strSnow == "on" || show.strSnow == "off");
		if (wants)
		{
			if (s_bPlayerBotDanceSkySet && s_strPlayerBotDanceSkyStamp == show.strStamp &&
					s_lPlayerBotDanceSkyMap == owner->GetMapIndex())
				return;
			if (show.strDayMode == "dark" || show.strDayMode == "light")
				owner->ChatPacket(CHAT_TYPE_COMMAND, "DayMode %s", show.strDayMode.c_str());
			if (show.strSnow == "on" || show.strSnow == "off")
				owner->ChatPacket(CHAT_TYPE_COMMAND, "xmas_snow %d", show.strSnow == "on" ? 1 : 0);
			s_bPlayerBotDanceSkySet = true;
			s_strPlayerBotDanceSkyStamp = show.strStamp;
			s_lPlayerBotDanceSkyMap = owner->GetMapIndex();
			return;
		}
		if (!s_bPlayerBotDanceSkySet)
			return;
		s_bPlayerBotDanceSkySet = false;
		s_strPlayerBotDanceSkyStamp.clear();
		const int day = quest::CQuestManager::instance().GetEventFlag("day");
		const int eclipse = quest::CQuestManager::instance().GetEventFlag("eclipse");
		owner->ChatPacket(CHAT_TYPE_COMMAND, "DayMode %s", day || eclipse == 1 ? "dark" : "light");
		owner->ChatPacket(CHAT_TYPE_COMMAND, "xmas_snow %d", quest::CQuestManager::instance().GetEventFlag("xmas_snow"));
	}

	// The owner's own core: where the owner stands (local cm, as /where and
	// the panel count), for the panel's "here, where I stand".
	void WritePlayerBotDanceOwnerSpot(LPCHARACTER owner, DWORD dwNow)
	{
		if (!owner || dwNow < s_dwPlayerBotDanceNextOwnerNote)
			return;
		s_dwPlayerBotDanceNextOwnerNote = dwNow + 2000;
		PIXEL_POSITION base;
		if (!SECTREE_MANAGER::instance().GetMapBasePositionByMapIndex(owner->GetMapIndex(), base))
			return;
		char path[256];
		snprintf(path, sizeof(path), "%s.owner.new.%u", PLAYERBOT_DANCE_STATUS_PATH, (unsigned int)g_bChannel);
		FILE* fp = fopen(path, "w");
		if (!fp)
			return;
		fprintf(fp, "time %ld\nowner %u\nname %s\nchannel %u\nmap %ld\nx %ld\ny %ld\nrot %.0f\n", (long)time(0),
				owner->GetPlayerID(), owner->GetName(), (unsigned int)g_bChannel, owner->GetMapIndex(),
				owner->GetX() - base.x, owner->GetY() - base.y, owner->GetRotation());
		fclose(fp);
		chmod(path, 0664);
		char target[256];
		snprintf(target, sizeof(target), "%s.owner", PLAYERBOT_DANCE_STATUS_PATH);
		rename(path, target);
	}

	void WritePlayerBotDanceStatus(DWORD dwNow, int present, int placed, bool speaker)
	{
		if (dwNow < s_dwPlayerBotDanceNextStatus)
			return;
		s_dwPlayerBotDanceNextStatus = dwNow + 1000;
		const TPlayerBotDanceShow& show = s_kPlayerBotDanceShow;
		// Only the core that holds the show speaks for it; another core
		// would overwrite it with "owner absent" every second.
		if (!speaker)
			return;
		char path[256];
		snprintf(path, sizeof(path), "%s.new.%u", PLAYERBOT_DANCE_STATUS_PATH, (unsigned int)g_bChannel);
		FILE* fp = fopen(path, "w");
		if (!fp)
			return;
		const TPlayerBotDanceAnchor& a = s_kPlayerBotDanceAnchor;
		fprintf(fp, "time %ld\nchannel %u\nstamp %s\ncommand %s\nowner %u\nwanted %d\npresent %d\nplaced %d\n"
				"beats %u\nlast_beat %ld\nnote %s\nquiet %d\npopulation %d\nphase %s\nformation %s\nstep %u\nsteps %u\n"
				"anchor_map %ld\nwalking %u\n",
				(long)time(0), (unsigned int)g_bChannel, show.strStamp.c_str(), show.strCommand.c_str(), show.dwOwner,
				show.iCount, present, placed, s_dwPlayerBotDanceBeats, (long)s_tPlayerBotDanceLastBeat,
				s_strPlayerBotDanceNote.empty() ? "-" : s_strPlayerBotDanceNote.c_str(),
				show.bQuiet ? 1 : 0, s_iPlayerBotDancePopulation, s_strPlayerBotDancePhase.c_str(),
				s_strPlayerBotDanceFormationNow.empty() ? "-" : s_strPlayerBotDanceFormationNow.c_str(),
				(unsigned int)s_nPlayerBotDanceNextStep, (unsigned int)show.vecSteps.size(),
				a.strStamp.empty() ? 0L : a.lMap, (unsigned int)s_mapPlayerBotDancerWalks.size());
		fclose(fp);
		chmod(path, 0664);
		rename(path, PLAYERBOT_DANCE_STATUS_PATH);
	}

	// With a spot the show belongs to the core that hosts its map, of the
	// owner's channel (each channel's core hosts the same maps).
	bool IsPlayerBotDanceSpotHere(const TPlayerBotDanceShow& show, LPCHARACTER owner)
	{
		if (!show.bSpot || !SECTREE_MANAGER::instance().GetMap(show.lMap))
			return false;
		if (owner)
			return true;
		CCI* cci = show.dwOwner ? P2P_MANAGER::instance().FindByPID(show.dwOwner) : NULL;
		return cci && cci->bChannel == g_bChannel;
	}

	// Where a dancer saved elsewhere loads: on the anchor of the show this
	// core holds (PlaceLoadingSidekick).
	bool GetPlayerBotDanceLoadSpot(long& lMapIndex, long& x, long& y)
	{
		if (s_kPlayerBotDanceAnchor.strStamp.empty() || !SECTREE_MANAGER::instance().GetMap(s_kPlayerBotDanceAnchor.lMap))
			return false;
		lMapIndex = s_kPlayerBotDanceAnchor.lMap;
		x = s_kPlayerBotDanceAnchor.lX;
		y = s_kPlayerBotDanceAnchor.lY;
		return true;
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
		// With a spot the owner may stand on another core of the channel.
		if (owner || (show.bSpot && show.dwOwner && P2P_MANAGER::instance().FindByPID(show.dwOwner) &&
				P2P_MANAGER::instance().FindByPID(show.dwOwner)->bChannel == g_bChannel))
			s_dwPlayerBotDanceOwnerSeen = dwNow;
		const bool wants = show.strCommand == "gather" || show.strCommand == "start";
		const bool here = show.bSpot ? IsPlayerBotDanceSpotHere(show, owner) : owner != NULL;
		const bool active = wants && here && PreparePlayerBotDanceAnchor(show, owner);
		if (!active && !(wants && here))
			ClearPlayerBotDanceAnchor();
		WritePlayerBotDanceOwnerSpot(owner, dwNow);
		SetPlayerBotDanceSky(show, owner, active);

		// Who should stand in the troupe now: the first `count` of the roster,
		// in their places (0 for a row still without a character).
		std::set<DWORD> wanted;
		std::vector<DWORD> order;
		std::vector<const TPlayerBotDancer*> rows;
		int present = 0, placed = 0;
		char placing[16];
		snprintf(placing, sizeof(placing), "%u", s_dwPlayerBotDancePlacing);
		if (active)
		{
			const int total = std::min<int>(show.iCount, (int)s_vecPlayerBotDancers.size());
			for (int n = 0; n < total; ++n)
			{
				TPlayerBotDancer& d = s_vecPlayerBotDancers[n];
				if (d.dwPid == 0 && dwNow >= s_dwPlayerBotDancerNextClaim)
				{
					// One claim every two seconds at most: each is a query over
					// the seed, and a row that cannot be served would ask again
					// on every pass.
					s_dwPlayerBotDancerNextClaim = dwNow + 2000;
					ClaimPlayerBotDancerIdentity(d, GetPlayerBotDanceOwnerEmpire(owner, show.dwOwner));
				}
				order.push_back(d.dwPid);
				rows.push_back(&d);
			}
			for (int n = 0; n < total; ++n)
			{
				const TPlayerBotDancer& d = *rows[n];
				if (d.dwPid == 0)
					continue;
				wanted.insert(d.dwPid);
				const bool managed = CPlayerBotManager::instance().IsManaged(d.dwPid);
				LPCHARACTER ch = managed ? CHARACTER_MANAGER::instance().FindByPID(d.dwPid) : NULL;
				if (!managed)
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
				if (mark != s_mapPlayerBotDancerPlaced.end() && mark->second == placing &&
						ch->GetMapIndex() == s_kPlayerBotDanceAnchor.lMap)
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
				float rot = 0;
				int row = 0;
				GetPlayerBotDancerSpotNow(n, total, &d, x, y, rot, row);
				if (JumpPlayerBotDancer(ch, x, y, rot, dwNow))
				{
					s_mapPlayerBotDancerPlaced[d.dwPid] = placing;
					SetPlayerBotDancerStatus(d.dwRow, "w szyku");
					++placed;
				}
			}
		}

		// Everybody of the troupe in this world who should not be: the show is
		// over, the owner left, the count went down or the row was deleted.
		// The owner gone a while (not a warp between two maps) ends it; a show
		// that is on but not acted on here keeps them for that while.
		const bool ownerSeenNow = s_dwPlayerBotDanceOwnerSeen == dwNow;
		const bool ownerGone = !ownerSeenNow && (s_dwPlayerBotDanceOwnerSeen == 0 ||
				dwNow - s_dwPlayerBotDanceOwnerSeen >= PLAYERBOT_DANCE_OWNER_GONE_MS);
		const bool hold = wants && !active && !ownerGone && !(show.bSpot && owner);
		// A dancer this core never placed counts too (stopped while it loaded).
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
				s_mapPlayerBotDancerWalks.erase(pid);
				continue;
			}
			if (active && wanted.count(pid))
				continue;
			if (hold)
				continue;
			LogOutPlayerBotDancer(pid, show.strCommand == "stop" ? "stop" : ownerGone ? "owner_gone" : "not_wanted");
			s_mapPlayerBotDancerWalks.erase(pid);
			s_mapPlayerBotDanceLoops.erase(pid);
		}

		if (active && show.strCommand == "start")
			RunPlayerBotDanceTimeline(show, owner, order, rows, dwNow);
		else if (show.strCommand != "start" || !active)
		{
			StopPlayerBotDanceTimeline();
			if (active)
				s_strPlayerBotDancePhase = "gather";
		}

		QuietPlayerBotWorld();
		s_strPlayerBotDanceNote = !show.dwOwner ? "no_owner" : show.bSpot && !here ? "spot_not_here" :
			!owner && !show.bSpot ? "owner_not_here" : "ok";
		WritePlayerBotDanceStatus(dwNow, present, placed, show.bSpot ? here : owner != NULL);
	}

	// The bot tick for a dancer: stand still, or walk to its new spot in the
	// formation and turn the troupe's way there.
	void KeepPlayerBotDancer(LPCHARACTER ch, TPlayerBotAIState& state, DWORD dwNow)
	{
		state.dwTargetVID = 0;
		ch->SetVictim(NULL);
		state.dwLastMeaningfulActivityTime = dwNow;
		std::map<DWORD, TPlayerBotDanceWalk>::iterator walk = s_mapPlayerBotDancerWalks.find(ch->GetPlayerID());
		if (walk == s_mapPlayerBotDancerWalks.end())
		{
			if (ch->IsStateMove())
				ch->Stop();
			return;
		}
		const TPlayerBotDanceWalk& w = walk->second;
		const long dist = DISTANCE_APPROX(ch->GetX() - w.lX, ch->GetY() - w.lY);
		if (ch->IsStateMove() && dist > 30 && dwNow - w.dwSince < PLAYERBOT_DANCE_WALK_GIVE_UP_MS)
			return;
		const long x = w.lX, y = w.lY;
		const float rot = w.fRot;
		s_mapPlayerBotDancerWalks.erase(walk);
		if (dist > 150)
		{
			// Stuck on the way: the rest of the way at once.
			JumpPlayerBotDancer(ch, x, y, rot, dwNow);
			return;
		}
		ch->Stop();
		ch->SetNowWalking(false);
		ch->SetRotation(rot);
		ch->SendMovePacket(FUNC_MOVE, 0, ch->GetX(), ch->GetY(), 0, dwNow);
	}
}
