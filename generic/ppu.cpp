#include "ppu.h"
#include "frontend.h"
#include "log.h"

#include <memory>
#include <array>
#include <algorithm>

using color_t = u16;


namespace ppu {

struct BGData {
  u16 hofs;
  u16 vofs;
  u16 bgcnt;
};

struct Pixel {
  u16 color;
  bool filled;
};

BGData GetBGData(u32 bg) {
  switch (bg) {
    case 0: return { REG_BG0HOFS, REG_BG0VOFS, REG_BG0CNT };
    case 1: return { REG_BG1HOFS, REG_BG1VOFS, REG_BG1CNT };
    case 2: return { REG_BG2HOFS, REG_BG2VOFS, REG_BG2CNT };
    case 3: return { REG_BG3HOFS, REG_BG3VOFS, REG_BG3CNT };
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


void Render4bpp(Pixel* dest, int start_x, const int x_sign, u8* tile_line_base, u8* palette_base) {
  int screen_x = start_x;

  for (int dx = 0; dx < 4; dx++) {
    for (int ddx = 0; ddx < 2; ddx++, screen_x += x_sign) {
      if (screen_x < 0 || screen_x >= frontend::GbaWidth) continue;  // out of bounds
      if (dest[screen_x].filled) continue;

      u8 vram_entry = tile_line_base[dx];
      u8 palette_nibble = (ddx == 1) ? (vram_entry >> 4) : (vram_entry & 0xf);

      // todo: check window
      if (palette_nibble) {
        dest[screen_x].color = ((vu16*)palette_base)[palette_nibble];
        dest[screen_x].filled = true;
      }
    }
  }
}

void Render8bpp(Pixel* dest, int start_x, const int x_sign, u8* tile_line_base, u8* palette_base) {
  int screen_x = start_x;

  for (int dx = 0; dx < 8; dx++, screen_x += x_sign) {
    if (screen_x < 0 || screen_x >= frontend::GbaWidth) continue;  // out of bounds
    if (dest[screen_x].filled) continue;

    u8 vram_entry = tile_line_base[dx];

    // todo: check window
    if (vram_entry) {
      dest[screen_x].color = ((vu16*)palette_base)[vram_entry];
      dest[screen_x].filled = true;
    }
  }
}

void RenderRegularScanline(u32 bg, u32 scanline, Pixel* dest) {
  if (!(REG_DISPCNT & (0x0100 << bg))) {
    // disabled in dispcnt
    return;
  }

  auto bg_data = GetBGData(bg);
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

      Render4bpp(dest, start_x, x_sign, &mem_vram[address], &mem_pltt[palette_bank * 0x20]);
    }
    else {
      // 8bpp
      address += tile_id * 0x40;  // beginning of tile
      address += dy * 8;          // beginning of tile sliver

      Render8bpp(dest, start_x, x_sign, &mem_vram[address], mem_pltt);
    }
  }
}

void ComposeScanline(color_t* dest, Pixel* scanline) {
  u16 backdrop = *(vu16*)mem_pltt;
  for (int i = 0; i < frontend::GbaWidth; i++) {
    if (scanline[i].filled) {
      dest[i] = scanline[i].color;
    }
    else {
      // fill rest with backdrop
      dest[i] = backdrop;
    }
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