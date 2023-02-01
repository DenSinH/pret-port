#include "ppu.h"
#include "frontend.h"
#include "log.h"

#include <memory>
#include <array>
#include <algorithm>

using color_t = u16;


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

BGData GetBGData(u32 bg) {
  switch (bg) {
    case 0: return { REG_BG0HOFS, REG_BG0VOFS, REG_BG0CNT, (REG_BLDCNT & (1 << bg)) != 0, (REG_BLDCNT & (0x100 << bg)) != 0};
    case 1: return { REG_BG1HOFS, REG_BG1VOFS, REG_BG1CNT, (REG_BLDCNT & (1 << bg)) != 0, (REG_BLDCNT & (0x100 << bg)) != 0 };
    case 2: return { REG_BG2HOFS, REG_BG2VOFS, REG_BG2CNT, (REG_BLDCNT & (1 << bg)) != 0, (REG_BLDCNT & (0x100 << bg)) != 0 };
    case 3: return { REG_BG3HOFS, REG_BG3VOFS, REG_BG3CNT, (REG_BLDCNT & (1 << bg)) != 0, (REG_BLDCNT & (0x100 << bg)) != 0 };
    default: log_fatal("Invalid background: %d", bg);
  }
}

u32 VramIndexRegular(u32 tile_x, u32 tile_y, u32 screen_block_size) {
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


void Render4bpp(Pixel* dest, const BGData& bg, int start_x, const int x_sign, u8* tile_line_base, u8* palette_base) {
  int screen_x = start_x;

  for (int dx = 0; dx < 8; dx++, screen_x += x_sign) {
    if (screen_x < 0 || screen_x >= frontend::GbaWidth) {
      if ((x_sign < 0) == (screen_x < 0)) return;
      continue;
    }
    if (dest[screen_x].IsFilled()) continue;

    u8 vram_entry = tile_line_base[dx >> 1];
    u8 palette_nibble = (dx & 1) ? (vram_entry >> 4) : (vram_entry & 0xf);

    // todo: check window
    if (palette_nibble) {
      dest[screen_x].SetColor(((vu16*)palette_base)[palette_nibble], bg.blend_top, bg.blend_bottom);
    }
  }
}

void Render8bpp(Pixel* dest, const BGData& bg, int start_x, const int x_sign, u8* tile_line_base, u8* palette_base) {
  int screen_x = start_x;

  for (int dx = 0; dx < 8; dx++, screen_x += x_sign) {
    if (screen_x < 0 || screen_x >= frontend::GbaWidth) {
      if ((x_sign < 0) == (screen_x < 0)) return;
      continue;
    }
    if (dest[screen_x].IsFilled()) continue;

    u8 vram_entry = tile_line_base[dx];

    // todo: check window
    if (vram_entry) {
      dest[screen_x].SetColor(((vu16*)palette_base)[vram_entry], bg.blend_top, bg.blend_bottom);
    }
  }
}

void RenderRegularScanline(u32 bg, u32 scanline, Pixel* dest) {
  if (!(REG_DISPCNT & (0x0100 << bg))) {
    // disabled in dispcnt
    return;
  }

  const auto bg_data = GetBGData(bg);
  u32 char_base_block   = (bg_data.bgcnt >> 2) & 3;
  u32 color_mode        = (bg_data.bgcnt >> 7) & 1;
  u32 screen_base_block = (bg_data.bgcnt >> 8) & 0x1f;
  u32 screen_size       = (bg_data.bgcnt >> 14) & 3;

  u32 effective_y = scanline + bg_data.vofs;
  // todo: mosaic

  for (int course_x = -1; course_x < 31; course_x++) {
    u32 effective_x = (course_x << 3) + bg_data.hofs;
    // todo: mosaic

    u32 screen_entry_index = VramIndexRegular(effective_x >> 3, effective_y >> 3, screen_size);
    screen_entry_index += screen_base_block * 0x800;
    u32 screen_entry = (((u16)mem_vram[screen_entry_index + 1]) << 8) | mem_vram[screen_entry_index];

    // get data for screen entry
    u32 palette_bank = (screen_entry >> 12) & 0xf;
    bool vflip       = (screen_entry & 0x0800) != 0;
    bool hflip       = (screen_entry & 0x0400) != 0;
    u32 tile_id      = (screen_entry & 0x03ff);

    u32 dy      = effective_y & 7;
    int start_x = (course_x << 3) - (bg_data.hofs & 7);
    u32 address = char_base_block * 0x4000;

    if (vflip) dy ^= 7;

    int x_sign = 1;
    if (hflip) {
      start_x += 7;
      x_sign = -1;
    }

    if (!color_mode) {
      // 4bpp
      address += tile_id * 0x20;  // beginning of tile
      address += dy * 4;          // beginning of tile sliver

      Render4bpp(dest, bg_data, start_x, x_sign, &mem_vram[address], &mem_pltt[palette_bank * 0x20]);
    }
    else {
      // 8bpp
      address += tile_id * 0x40;  // beginning of tile
      address += dy * 8;          // beginning of tile sliver

      Render8bpp(dest, bg_data, start_x, x_sign, &mem_vram[address], mem_pltt);
    }
  }
}

void ComposeScanline(color_t* dest, Pixel* scanline) {
  const u16 bldcnt = REG_BLDCNT;
  auto blend_mode = static_cast<BlendMode>((bldcnt >> 6) & 3);
  bool backdrop_top   = (bldcnt >> 5) & 1;
  bool backdrop_bottom = (bldcnt >> 13) & 1;

  const u16 bldalpha = REG_BLDALPHA;
  u16 eva = std::clamp<u16>(bldalpha & 0x1f, 0, 16);
  u16 evb = std::clamp<u16>((bldalpha >> 8) & 0x1f, 0, 16);
  u16 evy = std::clamp<u16>(REG_BLDY & 0x1f, 0, 16);

  u16 backdrop = *(vu16*)mem_pltt;
  for (int i = 0; i < frontend::GbaWidth; i++) {
    dest[i] = scanline[i].GetColor(blend_mode, backdrop, backdrop_top, backdrop_bottom, eva, evb, evy);
  }
}

void RenderScanline(u32 y, color_t* dest) {
  u16 mode = REG_DISPCNT & 0x3;
  Pixel scanline[frontend::GbaWidth] = {};

  switch (mode) {
    case 0: {
      // order layers by priority
      std::array<u32, 4> layers = { 0, 1, 2, 3 };
      const std::array<u32, 4> bgcnt = {
          REG_BG0CNT, REG_BG1CNT, REG_BG2CNT, REG_BG3CNT
      };
      std::sort(layers.begin(), layers.end(), [&](auto l, auto r) {
        if ((bgcnt[l] & 3) == (bgcnt[r] & 3)) {
          return l < r;
        }
        return (bgcnt[l] & 3) < (bgcnt[r] & 3);
      });

      RenderRegularScanline(layers[0], y, scanline);
      RenderRegularScanline(layers[1], y, scanline);
      RenderRegularScanline(layers[2], y, scanline);
      RenderRegularScanline(layers[3], y, scanline);
      break;
    }
    case 1: {
      // order layers by priority
      std::array<u32, 3> layers = { 0, 1, 2 };
      const std::array<u32, 3> bgcnt = {
          REG_BG0CNT, REG_BG1CNT, REG_BG2CNT
      };
      std::sort(layers.begin(), layers.end(), [&](auto l, auto r) {
        if ((bgcnt[l] & 3) == (bgcnt[r] & 3)) {
          return l < r;
        }
        return (bgcnt[l] & 3) < (bgcnt[r] & 3);
      });

      for (const auto& layer: layers) {
        if (layer == 2) {
          // affine layer
        }
        else {
          RenderRegularScanline(layer, y, scanline);
        }
      }
      break;
    }
    default: {
      log_warn("Unimplemented rendering mode: %d", mode);
      break;
    }
  }

  ComposeScanline(dest, scanline);
}

void RenderFrame(color_t* screen) {
  for (int i = 0; i < frontend::GbaHeight; i++) {
    RenderScanline(i, screen + i * frontend::GbaWidth);
  }
}

}