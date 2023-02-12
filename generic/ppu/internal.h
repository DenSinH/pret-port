#pragma once

#include "log.h"

namespace ppu {

enum class BlendMode : u32 {
  Off = 0,
  Normal = 1,
  White = 2,
  Black = 3,
};

struct BGData {
  u16 hofs;
  u16 vofs;
  u16 bgcnt;
  bool blend_top;
  bool blend_bottom;

  struct {
    s32 ref_x;
    s32 ref_y;
    s16 pa;
    s16 pb;
    s16 pc;
    s16 pd;
  } rot_scale;
};

struct ObjSize {
  u32 width;
  u32 height;
};

struct Pixel {
private:
  struct {
    u16 color;
    bool blend;
  } layers[2];
  uint_fast8_t filled;

  static u16 Blend(u16 color_a, u16 color_b, u16 eva, u16 evb) {
    u16 blend = 0;

    // colors in BGR555 format
    u16 bgr;

    // Blend red
    bgr = (u16)(((color_a & 0x001f) * eva + (color_b & 0x001f) * evb) >> 4);                  // 1.4 fixed point
    blend |= (u16)(bgr >= 0x1f ? 0x001f : (bgr << 0));

    // Blend green
    bgr = (u16)((((color_a & 0x03e0) >> 5) * eva + ((color_b & 0x03e0) >> 5) * evb) >> 4);    // 1.4 fixed point
    blend |= (u16)(bgr >= 0x1f ? 0x03e0 : (bgr << 5));

    // Blend blue
    bgr = (u16)((((color_a & 0x7c00) >> 10) * eva + ((color_b & 0x7c00) >> 10) * evb) >> 4);  // 1.4 fixed point
    blend |= (u16)(bgr >= 0x1f ? 0x7c00 : (bgr << 10));

    return blend;
  }

public:
  inline bool IsFilled() {
    switch(filled) {
      case 0: return false;
      case 1: return !layers[0].blend;
      case 2:
      default: return true;
    }
  }

  inline void SetColor(u16 color, bool blend_top, bool blend_bottom) {
    switch(filled) {
      case 0:
        layers[0].color = color;
        layers[0].blend = blend_top;
        // if layer does not blend, then we are done
        filled = blend_top ? 1 : 2;
        break;
      case 1:
        if (blend_bottom) {
          layers[1].color = color;
        }
        layers[1].blend = blend_bottom;
        filled = 2;
      case 2:
      default:
        return;
    }
  }

  inline u16 GetColor(BlendMode mode, u16 backdrop, bool backdrop_top, bool backdrop_bottom, u16 eva, u16 evb, u16 evy) {
    switch (mode) {
      case BlendMode::Off:
        return filled > 0 ? layers[0].color : backdrop;
      case BlendMode::Normal:
        switch(filled) {
          case 0: return backdrop;
          case 1:
            if (layers[0].blend && backdrop_bottom) {
              return Blend(layers[0].color, backdrop, eva, evb);
            }
            return layers[0].color;
          case 2:
            if (layers[0].blend && layers[1].blend) {
              return Blend(layers[0].color, layers[1].color, eva, evb);
            }
            return layers[0].color;
          default:
            log_fatal("Invalid filled value");
        }
      case BlendMode::White:
        if (filled > 0) [[likely]] {
          return Blend(layers[0].color, 0x7fff, 0x10 - evy, evy);
        }
        else {
          if (backdrop_top) {
            return Blend(backdrop, 0x7fff, 0x10 - evy, evy);
          }
          return backdrop;
        }
      case BlendMode::Black:
        if (filled > 0) [[likely]] {
          return Blend(layers[0].color, 0x0000, 0x10 - evy, evy);
        }
        else {
          if (backdrop_top) {
            return Blend(backdrop, 0x0000, 0x10 - evy, evy);
          }
          return backdrop;
        }
    }
  }
};

static BGData GetBGData(u32 bg) {
  switch (bg) {
    case 0: return {
          REG_BG0HOFS,
          REG_BG0VOFS,
          REG_BG0CNT,
          (REG_BLDCNT & (1 << bg)) != 0,
          (REG_BLDCNT & (0x100 << bg)) != 0,
          {}  // no affine parameters for layer
      };
    case 1: return {
          REG_BG1HOFS,
          REG_BG1VOFS,
          REG_BG1CNT,
          (REG_BLDCNT & (1 << bg)) != 0,
          (REG_BLDCNT & (0x100 << bg)) != 0,
          {}  // no affine parameters for layer
      };
    case 2: return {
          REG_BG2HOFS,
          REG_BG2VOFS,
          REG_BG2CNT,
          (REG_BLDCNT & (1 << bg)) != 0,
          (REG_BLDCNT & (0x100 << bg)) != 0,
          {
              (s32)(REG_BG2X << 4) >> 4,
              (s32)(REG_BG2Y << 4) >> 4,
              (s16)REG_BG2PA, (s16)REG_BG2PB, (s16)REG_BG2PC, (s16)REG_BG2PD
          }
      };
    case 3: return {
          REG_BG3HOFS,
          REG_BG3VOFS,
          REG_BG3CNT,
          (REG_BLDCNT & (1 << bg)) != 0,
          (REG_BLDCNT & (0x100 << bg)) != 0,
          {
              (s32)(REG_BG2X << 4) >> 4,
              (s32)(REG_BG2Y << 4) >> 4,
              (s16)REG_BG3PA, (s16)REG_BG3PB, (s16)REG_BG3PC, (s16)REG_BG3PD
          }
      };
    default: log_fatal("Invalid background: %d", bg);
  }
}

static u32 VramIndexRegular(u32 tile_x, u32 tile_y, u32 screen_block_size) {
  switch (screen_block_size) {

    case 0b00:  // 32x32
      return (u32)(((tile_y & 0x1f) << 6) | ((tile_x & 0x1f) << 1));
    case 0b01:  // 64x32
      return (u32)(((tile_x & 0x3f) > 31 ? 0x800 : 0) | ((tile_y & 0x1f) << 6) | ((tile_x & 0x1f) << 1));
    case 0b10:  // 32x64
      return (u32)(((tile_y & 0x3f) > 31 ? 0x800 : 0) | ((tile_y & 0x1f) << 6) | ((tile_x & 0x1f) << 1));
    case 0b11:  // 64x64
      return (u32)(((tile_y & 0x3f) > 31 ? 0x1000 : 0) | ((tile_x & 0x3f) > 31 ? 0x800 : 0) | ((tile_y & 0x1f) << 6) | ((tile_x & 0x1f) << 1));
    default:
      log_fatal("Invalid screen block size: %d", screen_block_size);
  }
}

static constexpr ObjSize ObjSizeTable[3][4] = {
    { {8, 8},  {16, 16}, {32, 32}, {64, 64} },
    { {16, 8}, {32, 8},  {32, 16}, {64, 32} },
    { {8, 16}, {8, 32},  {16, 32}, {32, 64} }
};

void RenderScanline(u32 scanline, color_t* dest);

}
