#ifndef __INC_METIN2_COMMON_DEFINES_H__
#define __INC_METIN2_COMMON_DEFINES_H__
#pragma once
//////////////////////////////////////////////////////////////////////////
// ### Standard Features ###
//#define _IMPROVED_PACKET_ENCRYPTION_
#ifdef _IMPROVED_PACKET_ENCRYPTION_
#define USE_IMPROVED_PACKET_DECRYPTED_BUFFER
#endif
#define __UDP_BLOCK__
//#define ENABLE_QUEST_CATEGORY

// ### END Standard Features ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### New Features ###
//#define ENABLE_NO_MOUNT_CHECK
#define ENABLE_D_NJGUILD
#define ENABLE_FULL_NOTICE
#define ENABLE_NEWSTUFF
#define ENABLE_PORT_SECURITY
//#define ENABLE_BELT_INVENTORY_EX
#define ENABLE_CMD_WARP_IN_DUNGEON
#define ENABLE_ITEM_ATTR_COSTUME // costume bonus sets (custom-patches/gf26_costume_bonus)
//#define ENABLE_SEQUENCE_SYSTEM
//#define ENABLE_PLAYER_PER_ACCOUNT5
//#define ENABLE_DICE_SYSTEM
#define ENABLE_EXTEND_INVEN_SYSTEM
// Enabled 16 September 2026 (Mount System integration, custom-patches/README.md).
#define ENABLE_MOUNT_COSTUME_SYSTEM
// ENABLE_ACCE_COSTUME_SYSTEM (below, in the Ex Features section) has been on
// since 17 September 2026 -- see that flag's own dated comment for the
// confirmed WEAR_COSTUME_ACCE/WEAR_BELT/WEAR_PENDANT/WEAR_GLOVE numbers as of
// that change. ENABLE_WEAPON_COSTUME_SYSTEM below inserts one more slot
// (WEAR_COSTUME_WEAPON=24) ahead of WEAR_PENDANT/WEAR_GLOVE, shifting both up
// by one again (25/26) -- see that flag's own comment for the full audit
// trail and the one-time player.item pos 25->26, 24->25 migration this
// requires (custom-patches/death_ruler_slot_migration.sql).
// Prepared 17 September 2026 (Death Ruler weapon costume set, operator's
// explicit request -- server-side only, NOT yet deployed to the running
// game process: the operator's own instruction is to hold off on a
// production restart until the client is rebuilt with this same flag).
// Verified via a real standalone compile, not guessed: WEAR_COSTUME_BODY=19,
// WEAR_COSTUME_HAIR=20, WEAR_COSTUME_MOUNT=21, WEAR_COSTUME_ACCE=22,
// WEAR_BELT=23, WEAR_COSTUME_WEAPON=24, WEAR_PENDANT=25, WEAR_GLOVE=26,
// WEAR_MAX=32 (literal constant, unaffected -- same as when ACCE was added).
// COSTUME_BODY=0, COSTUME_HAIR=1, COSTUME_MOUNT=2, COSTUME_ACCE=3,
// COSTUME_WEAPON=4. DRAGON_SOUL_EQUIP_SLOT_START stays 167 -- it is computed
// from the literal WEAR_MAX_NUM=32 (common/length.h:91), not from the WEAR_*
// enum's own value count, so this slot insertion cannot move it (same reason
// it survived the ACCE change). Weapon-costume/real-weapon compatibility
// (equip-time subtype match, and blocking the mismatched pair) is handled
// entirely by existing, previously-dormant code gated on this same flag
// (char_item.cpp's CanEquipNow(), see custom-patches/README.md's Death Ruler
// section) -- no new compatibility logic was written, per the operator's
// explicit instruction to reuse the existing mechanism.
//
// Compiled clean as ON (docker compose build game, 17 September 2026,
// exit 0, full game+db+playerbot tree -- db and the playerbot code share
// this same build/binary, confirmed earlier this session via `ps -ef`
// inside the container: ./db, ./auth and every ./game channel are sibling
// processes from one image, not separate services). Left commented back OUT
// here before the actual restart that deploys the operator's separate,
// already-approved same-day request (halve the boss-chest Szarfa drop rate
// and add the new Death Ruler wings, 85101/85104, to the existing -- already
// live -- boss-kill/boss-chest drop pools) specifically so that restart does
// NOT also flip this flag live, per the operator's own standing instruction
// not to deploy ENABLE_WEAPON_COSTUME_SYSTEM until the client is rebuilt.
// Flipped back ON and deployed 17 September 2026, operator's explicit
// go-ahead ("wlaczaj ten update") given in direct reply to the report above
// -- nothing else needed to change, the whole weapon-costume set (49001-
// 49007) and the two body/hair costumes (41980/41981/45722) were already
// fully prepared (item_proto rows already inserted, custom-patches/
// death_ruler_item_proto_seed.sql, custom-patches/README.md's Death Ruler
// section). death_ruler_slot_migration.sql run immediately before this
// build with game/db/bots stopped, per the operator's own required order.
#define ENABLE_WEAPON_COSTUME_SYSTEM
#define ENABLE_QUEST_DIE_EVENT
#define ENABLE_QUEST_BOOT_EVENT
#define ENABLE_QUEST_DND_EVENT
#define ENABLE_PET_SYSTEM_EX
#define ENABLE_SKILL_FLAG_PARTY
// Disabled 17 September 2026 (Dragon Soul / Alchemy integration): this flag
// made DragonSoul_IsQualified() always return true, bypassing the level-30
// quest-gated qualification the brief explicitly requires ("kwalifikacja od
// 30 poziomu"). Only one call site (char_dragonsoul.cpp), confirmed isolated
// before disabling.
//#define ENABLE_NO_DSS_QUALIFICATION
// #define ENABLE_NO_SELL_PRICE_DIVIDED_BY_5
#define ENABLE_CHECK_SELL_PRICE
#define ENABLE_GOTO_LAG_FIX
//#define ENABLE_MOUNT_COSTUME_EX_SYSTEM
#define ENABLE_PENDANT_SYSTEM
//#define ENABLE_GLOVE_SYSTEM
#define ENABLE_MOVE_CHANNEL
//#define ENABLE_QUIVER_SYSTEM
#define ENABLE_REDUCED_ENTITY_VIEW
#define ENABLE_GUILD_TOKEN_AUTH
#define ENABLE_DS_GRADE_MYTH
// Added 17 September 2026 (Dragon Soul / Alchemy integration). Reference
// package uses __DS_SET__/__DS_CHANGE_ATTR__/__DS_7_SLOT__ (source/common/
// service.h) -- renamed to this project's ENABLE_* convention. No master
// __DRAGON_SOUL_SYSTEM__-style switch exists here because this server's
// Dragon Soul core (DragonSoul.cpp, char_dragonsoul.cpp, ...) already
// compiles unconditionally, unlike the reference project's fully optional
// system -- these three gate only the NEW functionality being added, same
// pattern as ENABLE_DS_GRADE_MYTH above.
#define ENABLE_DS_SET
#define ENABLE_DS_CHANGE_ATTR
#define ENABLE_DS_7_SLOT
#define ENABLE_DB_SQL_LOG
#define ENABLE_ITEM_SAFE_FLUSH

#define __PET_SYSTEM__
#ifdef __PET_SYSTEM__
#define USE_ACTIVE_PET_SEAL_EFFECT
#define PET_SEAL_ACTIVE_SOCKET_IDX 2
#define USE_PET_SEAL_ON_LOGIN
#endif

enum eCommonDefines {
	// MT2009_PLUS_MAP_ALLOW_48_V1: game1 of the unified layout hosts 33 maps
	// once the Temple of Ochao (209) sits beside the bots (m2-render-config);
	// every core and the db core are built from this one tree.
	MAP_ALLOW_LIMIT = 48, // 32 default
};

//#define ENABLE_WOLFMAN_CHARACTER
#ifdef ENABLE_WOLFMAN_CHARACTER
// #define DISABLE_WOLFMAN_ON_CREATE
#define USE_MOB_BLEEDING_AS_POISON
#define USE_MOB_CLAW_AS_DAGGER
// #define USE_ITEM_BLEEDING_AS_POISON
// #define USE_ITEM_CLAW_AS_DAGGER
#define USE_WOLFMAN_STONES
#define USE_WOLFMAN_BOOKS
#endif

// #define ENABLE_MAGIC_REDUCTION_SYSTEM
#ifdef ENABLE_MAGIC_REDUCTION_SYSTEM
// #define USE_MAGIC_REDUCTION_STONES
#endif

// ### END New Features ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### Ex Features ###
//#define DISABLE_STOP_RIDING_WHEN_DIE //	if DISABLE_TOP_RIDING_WHEN_DIE is defined, the player doesn't lose the horse after dying
// Enabled 17 September 2026 (Szarfa/sash accessory costume system,
// custom-patches/README.md). Client already compiled and confirmed with
// matching slot/point numbers (WEAR_COSTUME_ACCE=22, WEAR_BELT=23,
// WEAR_PENDANT=24, WEAR_GLOVE=25, POINT_ACCEDRAIN_RATE=177,
// POINT_MAX_NUM=178, HEADER_CG_ACCE=211, HEADER_GC_ACCE=215).
#define ENABLE_ACCE_COSTUME_SYSTEM //fixed version
// #define USE_ACCE_ABSORB_WITH_NO_NEGATIVE_BONUS //enable only positive bonus in acce absorb
#define ENABLE_HIGHLIGHT_NEW_ITEM //if you want to see highlighted a new item when dropped or when exchanged
#define ENABLE_KILL_EVENT_FIX //if you want to fix the 0 exp problem about the when kill lua event (recommended)
// #define ENABLE_SYSLOG_PACKET_SENT // debug purposes

#define ENABLE_EXTEND_ITEM_AWARD //slight adjustement
#ifdef ENABLE_EXTEND_ITEM_AWARD
	// #define USE_ITEM_AWARD_CHECK_ATTRIBUTES //it prevents bonuses higher than item_attr lvl1-lvl5 min-max range limit
#endif

//#define ENABLE_CHEQUE_SYSTEM
#ifdef ENABLE_CHEQUE_SYSTEM
#define ENABLE_SHOP_USE_CHEQUE
#define DISABLE_CHEQUE_DROP
#define ENABLE_WON_EXCHANGE_WINDOW
#endif
// ### END Ex Features ###
//////////////////////////////////////////////////////////////////////////

#define ENABLE_IKASHOP_RENEWAL
#define ENABLE_IKASHOP_ENTITIES
#define EXTEND_IKASHOP_PRO
#define EXTEND_IKASHOP_ULTIMATE
#define ENABLE_IKASHOP_LOGS
#define ENABLE_LARGE_DYNAMIC_PACKETS

#define ENABLE_MT2009_IKASHOP_RESTRICTIONS
#define ENABLE_MT2009_DISABLE_IKASHOP_DEFAULT_SAFEBOX

#define ENABLE_MT2009_DISABLE_SAFEBOX_STACK
#define DISABLE_WHISPER_PRISM_REQUIREMENT
#define DISABLE_GOLD_PLAYER_DROP
#define DISABLE_MT2009_EXCHANGE_ITEM_DEL
#define ENABLE_CHANNEL_STATUS_CACHE

#define __NEW_EVENT_HANDLER__
#endif
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by Metin2hub.com
