extern int BlueDragon_StateBattle (LPCHARACTER);
extern time_t UseBlueDragonSkill (LPCHARACTER, unsigned int);
extern int BlueDragon_Damage (LPCHARACTER me, LPCHARACTER attacker, int dam);
// MT2009_PLUS_BLUE_DRAGON_V1 (decl): the Blue Dragon lair (server-patches/bluedragon).
// Beran-Setaou (2493) in an instance of map 208 takes no damage while one of its four
// stones (8031-8034) stands there; each dragon keeps its own skill cooldowns.
extern bool BlueDragon_IsBoss (DWORD dwVnum);
extern bool BlueDragon_Block (long lMapIndex);
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
