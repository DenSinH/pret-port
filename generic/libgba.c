#include "gba/gba.h"

#include <memory.h>


void SoftReset(u32 resetFlags);

void RegisterRamReset(u32 resetFlags);

void VBlankIntrWait(void);

u16 Sqrt(u32 num);

u16 ArcTan2(s16 x, s16 y);

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

void CpuFastSet(const void *src, void *dest, u32 control);

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count);

void ObjAffineSet(struct ObjAffineSrcData *src, void *dest, s32 count, s32 offset);

void LZ77UnCompWram(const void *src, void *dest);

void LZ77UnCompVram(const void *src, void *dest);

void RLUnCompWram(const void *src, void *dest);

void RLUnCompVram(const void *src, void *dest);

int MultiBoot(struct MultiBootParam *mp);
