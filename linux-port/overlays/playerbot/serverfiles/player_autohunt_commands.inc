// MT2009_PLUS_AUTOHUNT_V2
// The player's auto-hunt (client-root/uiautohunt.py) asks which monster to go
// for. The client has no list of the characters round it - the scripts that
// do this without the server scan a million VIDs a frame - and the sectree
// has one. Monsters, and Metin stones when the window asks for them; only what
// battle_is_attackable lets this character hit; within the range of the point
// the hunt started from. What is already hitting the hunter comes first, then
// the nearest. The answer is "AutoHuntTarget <vid>", zero for nothing.
struct FAutoHuntTarget
{
	LPCHARACTER	m_ch;
	int		m_iAnchorX;
	int		m_iAnchorY;
	int		m_iRange;
	bool		m_bStones;
	DWORD		m_dwSkipVID;
	LPCHARACTER	m_pkBest;
	int		m_iBestScore;

	FAutoHuntTarget(LPCHARACTER ch, int anchorX, int anchorY, int range, bool stones, DWORD skipVID)
		: m_ch(ch), m_iAnchorX(anchorX), m_iAnchorY(anchorY), m_iRange(range), m_bStones(stones),
		m_dwSkipVID(skipVID),
		m_pkBest(NULL), m_iBestScore(0x7fffffff)
	{
	}

	void operator () (LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_CHARACTER))
			return;

		LPCHARACTER victim = (LPCHARACTER) ent;
		if (victim == m_ch || victim->IsDead() || (DWORD) victim->GetVID() == m_dwSkipVID)
			return;
		if (!victim->IsMonster() && !(m_bStones && victim->IsStone()))
			return;
		if (DISTANCE_APPROX(victim->GetX() - m_iAnchorX, victim->GetY() - m_iAnchorY) > m_iRange)
			return;
		if (!battle_is_attackable(m_ch, victim))
			return;

		int score = DISTANCE_APPROX(victim->GetX() - m_ch->GetX(), victim->GetY() - m_ch->GetY());
		if (victim->GetVictim() == m_ch)
			score /= 4;
		if (score < m_iBestScore)
		{
			m_iBestScore = score;
			m_pkBest = victim;
		}
	}
};

ACMD(do_autohunt_target)
{
	char arg1[256], arg2[256], arg3[256], arg4[256], arg5[256] = { 0 };
	const char * rest = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	const char * tail = two_arguments(rest, arg3, sizeof(arg3), arg4, sizeof(arg4));
	one_argument(tail, arg5, sizeof(arg5));

	if (!ch->GetSectree() || ch->IsDead())
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntTarget 0");
		return;
	}

	int range = 2000;
	int stones = 0;
	DWORD skipVID = 0;
	str_to_number(range, arg1);
	str_to_number(stones, arg2);
	str_to_number(skipVID, arg5);
	range = MAX(300, MIN(range, 5000));

	int anchorX = ch->GetX();
	int anchorY = ch->GetY();
	if (*arg3 && *arg4)
	{
		int x = 0;
		int y = 0;
		str_to_number(x, arg3);
		str_to_number(y, arg4);
		// The client counts a position from its own map's corner and the
		// server from the world's (the client's network stream takes the
		// map's base off every position it receives), so the start point
		// comes as an offset from where the character stands. One further
		// than the sectrees round the character reach is a stale hunt from
		// another place: hunt round the character instead.
		if (DISTANCE_APPROX(x, y) <= 10000)
		{
			anchorX += x;
			anchorY += y;
		}
	}

	FAutoHuntTarget f(ch, anchorX, anchorY, range, stones != 0, skipVID);
	ch->GetSectree()->ForEachAround(f);
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntTarget %u",
			f.m_pkBest ? (unsigned int) (DWORD) f.m_pkBest->GetVID() : 0);
}

// The auto-hunt's pick-up by kind (client-root/uiautohunt.py). The client's
// own PickCloseItem takes whatever lies nearest and cannot tell a sword from a
// potion, and the window offers "do not pick up weapons, armour, ..." (Tieru,
// 15 September). So the client asks "/autohunt_loot <range> <kinds> <x> <y>"
// and is answered "AutoHuntLoot <vid> <x> <y>": the nearest item on the ground
// this character may take, of a kind the window keeps, within the range of
// the point the hunt started from - zero for nothing. The client walks there
// and sends the ordinary pick-up packet, which CHARACTER::PickupItem judges as
// it judges anybody's. Yang goes with every kind.
enum
{
	AUTOHUNT_LOOT_WEAPON = 1 << 0,
	AUTOHUNT_LOOT_ARMOUR = 1 << 1,
	AUTOHUNT_LOOT_JEWELLERY = 1 << 2,
	AUTOHUNT_LOOT_POTION = 1 << 3,
	AUTOHUNT_LOOT_BOOK = 1 << 4,
	AUTOHUNT_LOOT_STONE = 1 << 5,
	AUTOHUNT_LOOT_OTHER = 1 << 6,
};

static int AutoHuntLootKind(LPITEM item)
{
	switch (item->GetType())
	{
		case ITEM_WEAPON:
			return item->GetSubType() == WEAPON_ARROW ? AUTOHUNT_LOOT_OTHER : AUTOHUNT_LOOT_WEAPON;
		case ITEM_ARMOR:
			switch (item->GetSubType())
			{
				case ARMOR_BODY:
				case ARMOR_HEAD:
				case ARMOR_SHIELD:
					return AUTOHUNT_LOOT_ARMOUR;
				default:
					return AUTOHUNT_LOOT_JEWELLERY;
			}
		case ITEM_RING:
		case ITEM_BELT:
			return AUTOHUNT_LOOT_JEWELLERY;
		case ITEM_USE:
			switch (item->GetSubType())
			{
				case USE_POTION:
				case USE_POTION_NODELAY:
				case USE_ABILITY_UP:
					return AUTOHUNT_LOOT_POTION;
				default:
					return AUTOHUNT_LOOT_OTHER;
			}
		case ITEM_SKILLBOOK:
		case ITEM_SKILLFORGET:
			return AUTOHUNT_LOOT_BOOK;
		case ITEM_METIN:
			return AUTOHUNT_LOOT_STONE;
		default:
			return AUTOHUNT_LOOT_OTHER;
	}
}

struct FAutoHuntLoot
{
	LPCHARACTER m_ch;
	int m_iAnchorX;
	int m_iAnchorY;
	int m_iRange;
	int m_iKinds;
	LPITEM m_pkBest;
	int m_iBestDistance;

	FAutoHuntLoot(LPCHARACTER ch, int anchorX, int anchorY, int range, int kinds)
		: m_ch(ch), m_iAnchorX(anchorX), m_iAnchorY(anchorY), m_iRange(range), m_iKinds(kinds),
		m_pkBest(NULL), m_iBestDistance(0x7fffffff)
	{
	}

	void operator () (LPENTITY ent)
	{
		if (!ent->IsType(ENTITY_ITEM))
			return;

		LPITEM item = (LPITEM) ent;
		if (item->GetOwner() || !item->GetSectree())
			return;
		if (item->GetType() != ITEM_ELK && !(AutoHuntLootKind(item) & m_iKinds))
			return;
		if (DISTANCE_APPROX(item->GetX() - m_iAnchorX, item->GetY() - m_iAnchorY) > m_iRange)
			return;
		if (!item->IsOwnership(m_ch))
			return;

		const int distance = DISTANCE_APPROX(item->GetX() - m_ch->GetX(), item->GetY() - m_ch->GetY());
		if (distance < m_iBestDistance)
		{
			m_iBestDistance = distance;
			m_pkBest = item;
		}
	}
};

ACMD(do_autohunt_loot)
{
	char arg1[256], arg2[256], arg3[256], arg4[256];
	const char * rest = two_arguments(argument, arg1, sizeof(arg1), arg2, sizeof(arg2));
	two_arguments(rest, arg3, sizeof(arg3), arg4, sizeof(arg4));

	int range = 2000;
	int kinds = 0;
	str_to_number(range, arg1);
	str_to_number(kinds, arg2);
	range = MAX(300, MIN(range, 5000));

	if (!ch->GetSectree() || ch->IsDead() || kinds <= 0)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntLoot 0 0 0");
		return;
	}

	int anchorX = ch->GetX();
	int anchorY = ch->GetY();
	if (*arg3 && *arg4)
	{
		int x = 0;
		int y = 0;
		str_to_number(x, arg3);
		str_to_number(y, arg4);
		// The same rule as the target: an offset from the character, and a
		// stale one hunts round the character.
		if (DISTANCE_APPROX(x, y) <= 10000)
		{
			anchorX += x;
			anchorY += y;
		}
	}

	FAutoHuntLoot f(ch, anchorX, anchorY, range, kinds);
	ch->GetSectree()->ForEachAround(f);
	if (!f.m_pkBest)
	{
		ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntLoot 0 0 0");
		return;
	}
	// The item's place as an offset from the character, which the client adds
	// to its own position: in the world's coordinates every item stood a
	// map's base away from a client that counts from its map's corner, and
	// the pick-up never came within reach (Tieru, 15 September).
	ch->ChatPacket(CHAT_TYPE_COMMAND, "AutoHuntLoot %u %ld %ld",
			(unsigned int) (DWORD) f.m_pkBest->GetVID(),
			(long) (f.m_pkBest->GetX() - ch->GetX()), (long) (f.m_pkBest->GetY() - ch->GetY()));
}
