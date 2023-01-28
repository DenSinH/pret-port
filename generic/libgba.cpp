#include "helpers.h"
#include "helpers.libgba.h"
#include "frontend.h"

#include <memory>
#include <cmath>

#include "log.h"

template<typename T>
void LZ77Uncomp(const void* _src, void* _dest) {
  // generic LZ77 handler
  const u8* src = (u8*)_src;
  u8* dest = (u8*)_dest;

  // todo: big endian host
  u32 header = *(u32*)src;
  src += 4;

  // todo: bios address check?
  u32 count = 0;
  u32 shift = 0;
  T buffer = 0;
  u32 len = header >> 8;

  auto check_count = [&]{
    if (count == sizeof(T)) {
      *(T*)dest = buffer;
      log_debug("Writing %04x to dest", buffer);
      dest += sizeof(T);
      count = 0;
      shift = 0;
      buffer = 0;
    }
  };

  while (len > 0) {
    u8 val = *src++;

    for (int i = 0; i < 8; i++) {
      if (val & 0x80) {
        u16 data = (u16)*src++ << 8;
        data |= *src++;

        u32 window_length = (data >> 12) + 3;
        u32 offset = data & 0x0fff;

        u8* window = dest + count - offset - 1;
        for (int j = 0; j < window_length; j++) {
          buffer |= (T)*window++ << shift;
          shift += 8;
          count++;

          check_count();

          if (!--len) {
            return;
          }
        }
      }
      else {
        buffer |= (T)*src++ << shift;
        shift += 8;
        count++;

        check_count();

        if (!--len) {
          return;
        }
      }

      val <<= 1;
    }
  }
}

extern "C" {

extern u32 intr_check;
extern u32 intr_vector;


void SoftReset(u32 resetFlags) {
  log_debug("SoftReset call");
  intr_check = 0;
  intr_vector = 0;
}

void RegisterRamReset(u32 resetFlags) {
  // we assume that RAM has been reset properly by the host anyway...
  // todo: reset emulated memory regions?
  log_debug("RegisterRamReset call with flags %08x, ignoring...", resetFlags);
}

void VBlankIntrWait(void) {
  log_warn("VBlankIntrWait call");
  frontend::RunFrame();
  nongeneric::HandleInterrupt(Interrupt::VBlank);
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
    u32* buf = static_cast<u32*>(dest);
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

void LZ77UnCompWram(const void *_src, void *_dest) {
  log_debug("LZ77UnCompWram call");
  LZ77Uncomp<u8>(_src, _dest);
}

void LZ77UnCompVram(const void *_src, void *_dest) {
  log_debug("LZ77UnCompVram call");
  LZ77Uncomp<u16>(_src, _dest);
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

}