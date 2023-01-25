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
u8 gMoveSelectionCursor[4] = {};
u8 gActionSelectionCursor[4] = {};
u8 gUnknown_02024E68[4] = {};
u32 gBattleControllerExecFlags = 0;
struct Struct20238C8 gUnknown_020238C8 = {};
struct ProtectStruct gProtectStructs[MAX_BATTLERS_COUNT] = {};
struct WishFutureKnock gWishFutureKnock = {};
struct BattleEnigmaBerry gEnigmaBerries[4] = {};
struct MultiBattlePokemonTx gMultiPartnerParty[3] = {};  // todo: check size
struct SpecialStatus gSpecialStatuses[MAX_BATTLERS_COUNT] = {};

u8 gPotentialItemEffectBattler = 0;
u8 gLastUsedAbility = 0;
u8 gMultiHitCounter = 0;
u8 gUnknown_02024C78 = 0;
u8 gBankInMenu = 0;
u8 gBattleOutcome = 0;
u8 gBattleCommunication = 0;
u8 gCurrentActionFuncId = 0;
u8 gCurrentTurnActionNumber = 0;
u8 gUnknown_02024BE5 = 0;
u8 gCurrMovePos = 0;

u16 gChosenMove = 0;
u16 gPauseCounterBattle = 0;
u16 gPaydayMoney = 0;
u16 gRandomTurnNumber = 0;

u32 gUnknown_020239FC = 0;
u32 gHitMarker = 0;

s32 gHpDealt = 0;

s32 gTakenDmg[MAX_BATTLERS_COUNT] = {};

u8 sUnusedBattlersArray[4] = {};
u8 gBattlerByTurnOrder[4] = {};
u8 gActionForBanks[4] = {};
u8 gActionsByTurnOrder[4] = {};
u8 gTakenDmgByBattler[4] = {};
u8 gUnknown_02024D1F[8] = {};

u8* gSelectionBattleScripts[4] = {};

u8* gBattlescriptCurrInstr = 0;

u8 gBattleBufferA[4][0x200] = {};
u8 gBattleBufferB[4][0x200] = {};
u8 gDisplayedStringBattle[300] = {};

u32 gTransformedPersonalities[4] = {};

u16 gLastPrintedMoves[4] = {};
u16 gLockedMoves[4] = {};
u16 gLastResultingMoves[4] = {};
u16 gLastHitByType[4] = {};
u16 gChosenMovesByBanks[4] = {};

/* asm file symbols */