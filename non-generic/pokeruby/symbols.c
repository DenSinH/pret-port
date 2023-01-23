#include "gba/gba.h"

#include "global.h"
#include "pokemon.h"
#include "battle.h"

/* eWRAM symbols
 *  Sizes are found in sym_ewram.txt
 * */
u16 gBattleTypeFlags = 0;
u16 gIntroSlideFlags = 0;
u16 gDynamicBasePower = 0;
u16 gMoveResultFlags = 0;
u16 gCurrentMove = 0;

u16 gLastMoves[4];

u8 gBattleMonForms[4] = {};
u8 gBattlerSpriteIds[4] = {};
u8 gBattlerPartyIndexes[8] = {};
u8 gBattlerPositions[4] = {};
u8 gBattlerAttacker = 0;
u8 gBattlerTarget = 0;
u8 gEffectBattler = 0;
u8 gBattlersCount = 0;
u8 gActiveBattler = 0;
u8 gAbsentBattlerFlags = 0;
u8 gCritMultiplier = 0;

struct BattlePokemon gBattleMons[0x160 / sizeof(struct BattlePokemon)];

u32 gStatuses3[MAX_BATTLERS_COUNT] = {};
u16 gSideStatuses[2] = {};
int gBattleMoveDamage = 0;
u16 gBattleWeather = 0;
struct DisableStruct gDisableStructs[MAX_BATTLERS_COUNT] = {};
struct SideTimer gSideTimers[2] = {};

u16 gLastLandedMoves = 0;
u8 gLastHitBy[MAX_BATTLERS_COUNT] = {};

u16 gLastUsedItem = 0;
u8 gDoingBattleAnim = 0;

/* asm file symbols */
// gMonSpriteGfx_Sprite_ptr
// gSpriteTemplate_81F96D0
// BattleAIs
// gTypeEffectiveness
// gSingingMoves
// gBattleAnims_Moves
// gAffineAnims_BattleSpriteContest
// gAffineAnims_BattleSpriteOpponentSide
// gBattleAnims_StatusConditions
// gUnknown_081F9680