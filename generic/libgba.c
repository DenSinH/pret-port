#include "gba/gba.h"

#include <memory.h>
#include <math.h>

#include "log.h"


void SoftReset(u32 resetFlags) {
  log_fatal("SoftReset call");
}

void RegisterRamReset(u32 resetFlags) {
  log_fatal("RegisterRamReset call");
}

void VBlankIntrWait(void) {
  log_fatal("VBlankIntrWait call");
}

u16 Sqrt(u32 num) {
  return (u16)(u32)sqrt(num);
}

u16 ArcTan2(s16 x, s16 y) {
  log_fatal("ArcTan2 call");
}

#define CPU_SET_SRC_FIXED 0x01000000
#define CPU_SET_16BIT     0x00000000
#define CPU_SET_32BIT     0x04000000

void CpuSet(const void *src, void *dest, u32 control) {
  u32 wordcount = control & 0x00ffffff;

  // forcefully round up to multiples of 8 words
  if (wordcount & 7) {
    wordcount = (wordcount & ~7) + 8;
  }

  if (control & CPU_SET_SRC_FIXED) {
    u32* buf = dest;
    u32 val = *(u32*)src;
    while (wordcount--) *buf++ = val;
  }
  else {
    memcpy(dest, src, wordcount * sizeof(u32));
  }
}

#define CPU_FAST_SET_SRC_FIXED 0x01000000

void CpuFastSet(const void *src, void *dest, u32 control) {
  log_fatal("CpuFastSet call");
}

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count) {
  log_fatal("BgAffineSet call");
}

void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset) {
  log_fatal("ObjAffineSet call");
}

void LZ77UnCompWram(const void *src, void *dest) {
  log_fatal("LZ77UnCompWram call");
}

void LZ77UnCompVram(const void *src, void *dest) {
  log_fatal("LZ77UnCompVram call");
}

void RLUnCompWram(const void *src, void *dest) {
  log_fatal("RLUnCompWram call");
}

void RLUnCompVram(const void *src, void *dest) {
  log_fatal("RLUnCompVram call");
}

int MultiBoot(struct MultiBootParam *mp) {
  log_fatal("MultiBoot call");
}

u32 umul3232H32(u32 multiplier, u32 multiplicand) {
  u64 result = (u64)multiplier * (u64)multiplicand;
  return (u32)(result >> 32);
}