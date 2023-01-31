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

  if (control & CPU_SET_32BIT) {
    if (control & CPU_SET_SRC_FIXED) {
      u32* buf = static_cast<u32*>(dest);
      u32 val = *(u32*)src;
      while (wordcount--) *buf++ = val;
    }
    else {
      memcpy(dest, src, wordcount * sizeof(u32));
    }
  }
  else {
    if (control & CPU_SET_SRC_FIXED) {
      u16* buf = static_cast<u16*>(dest);
      u16 val = *(u16*)src;
      while (wordcount--) *buf++ = val;
    }
    else {
      memcpy(dest, src, wordcount * sizeof(u16));
    }
  }
}

#define CPU_FAST_SET_SRC_FIXED 0x01000000

void CpuFastSet(const void *src, void *dest, u32 control) {
  log_fatal("CpuFastSet call");
}

const static u16 SinLut[] = {
    0x0000, 0x0192, 0x0323, 0x04B5, 0x0645, 0x07D5, 0x0964, 0x0AF1,
    0x0C7C, 0x0E05, 0x0F8C, 0x1111, 0x1294, 0x1413, 0x158F, 0x1708,
    0x187D, 0x19EF, 0x1B5D, 0x1CC6, 0x1E2B, 0x1F8B, 0x20E7, 0x223D,
    0x238E, 0x24DA, 0x261F, 0x275F, 0x2899, 0x29CD, 0x2AFA, 0x2C21,
    0x2D41, 0x2E5A, 0x2F6B, 0x3076, 0x3179, 0x3274, 0x3367, 0x3453,
    0x3536, 0x3612, 0x36E5, 0x37AF, 0x3871, 0x392A, 0x39DA, 0x3A82,
    0x3B20, 0x3BB6, 0x3C42, 0x3CC5, 0x3D3E, 0x3DAE, 0x3E14, 0x3E71,
    0x3EC5, 0x3F0E, 0x3F4E, 0x3F84, 0x3FB1, 0x3FD3, 0x3FEC, 0x3FFB,
    0x4000, 0x3FFB, 0x3FEC, 0x3FD3, 0x3FB1, 0x3F84, 0x3F4E, 0x3F0E,
    0x3EC5, 0x3E71, 0x3E14, 0x3DAE, 0x3D3E, 0x3CC5, 0x3C42, 0x3BB6,
    0x3B20, 0x3A82, 0x39DA, 0x392A, 0x3871, 0x37AF, 0x36E5, 0x3612,
    0x3536, 0x3453, 0x3367, 0x3274, 0x3179, 0x3076, 0x2F6B, 0x2E5A,
    0x2D41, 0x2C21, 0x2AFA, 0x29CD, 0x2899, 0x275F, 0x261F, 0x24DA,
    0x238E, 0x223D, 0x20E7, 0x1F8B, 0x1E2B, 0x1CC6, 0x1B5D, 0x19EF,
    0x187D, 0x1708, 0x158F, 0x1413, 0x1294, 0x1111, 0x0F8C, 0x0E05,
    0x0C7C, 0x0AF1, 0x0964, 0x07D5, 0x0645, 0x04B5, 0x0323, 0x0192,
    0x0000, 0xFE6E, 0xFCDD, 0xFB4B, 0xF9BB, 0xF82B, 0xF69C, 0xF50F,
    0xF384, 0xF1FB, 0xF074, 0xEEEF, 0xED6C, 0xEBED, 0xEA71, 0xE8F8,
    0xE783, 0xE611, 0xE4A3, 0xE33A, 0xE1D5, 0xE075, 0xDF19, 0xDDC3,
    0xDC72, 0xDB26, 0xD9E1, 0xD8A1, 0xD767, 0xD633, 0xD506, 0xD3DF,
    0xD2BF, 0xD1A6, 0xD095, 0xCF8A, 0xCE87, 0xCD8C, 0xCC99, 0xCBAD,
    0xCACA, 0xC9EE, 0xC91B, 0xC851, 0xC78F, 0xC6D6, 0xC626, 0xC57E,
    0xC4E0, 0xC44A, 0xC3BE, 0xC33B, 0xC2C2, 0xC252, 0xC1EC, 0xC18F,
    0xC13B, 0xC0F2, 0xC0B2, 0xC07C, 0xC04F, 0xC02D, 0xC014, 0xC005,
    0xC000, 0xC005, 0xC014, 0xC02D, 0xC04F, 0xC07C, 0xC0B2, 0xC0F2,
    0xC13B, 0xC18F, 0xC1EC, 0xC252, 0xC2C2, 0xC33B, 0xC3BE, 0xC44A,
    0xC4E0, 0xC57E, 0xC626, 0xC6D6, 0xC78F, 0xC851, 0xC91B, 0xC9EE,
    0xCACA, 0xCBAD, 0xCC99, 0xCD8C, 0xCE87, 0xCF8A, 0xD095, 0xD1A6,
    0xD2BF, 0xD3DF, 0xD506, 0xD633, 0xD767, 0xD8A1, 0xD9E1, 0xDB26,
    0xDC72, 0xDDC3, 0xDF19, 0xE075, 0xE1D5, 0xE33A, 0xE4A3, 0xE611,
    0xE783, 0xE8F8, 0xEA71, 0xEBED, 0xED6C, 0xEEEF, 0xF074, 0xF1FB,
    0xF384, 0xF50F, 0xF69C, 0xF82B, 0xF9BB, 0xFB4B, 0xFCDD, 0xFE6E,
};

void BgAffineSet(struct BgAffineSrcData *src, struct BgAffineDstData *dest, s32 count) {
  for (int i = 0; i < count; i++) {
    s32 a = (s16)SinLut[(src[i].alpha >> 8) + 0x40];
    s32 b = (s16)SinLut[src[i].alpha >> 8];

    dest[i].pa = (s16)(((s32)src[i].sx * a) >> 14);
    dest[i].pb = -(s16)(((s32)src[i].sx * b) >> 14);
    dest[i].pc = (s16)(((s32)src[i].sy * b) >> 14);
    dest[i].pd = (s16)(((s32)src[i].sy * a) >> 14);

    dest[i].dx = src[i].texX - dest[i].pa * src[i].scrX - dest[i].pb * src[i].scrY;
    dest[i].dy = src[i].texY - dest[i].pc * src[i].scrX - dest[i].pd * src[i].scrY;
  }
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
  // this is not used
  log_fatal("MultiBoot call");
}

u32 umul3232H32(u32 multiplier, u32 multiplicand) {
  u64 result = (u64)multiplier * (u64)multiplicand;
  return (u32)(result >> 32);
}

}