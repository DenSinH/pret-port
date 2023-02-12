#include "scanline.h"
#include "ppu.h"
#include "internal.h"
#include "frontend.h"

#include <memory>
#include <array>
#include <algorithm>
#include <vector>
#include <iterator>

#undef max
#undef min


namespace ppu {

static inline void Render4bpp(Pixel* dest, bool blend_top, bool blend_bottom, int start_x, const int x_sign, u8* tile_line_base, u8* palette_base) {
  const int clamped_start = std::clamp(start_x, 0, frontend::GbaWidth);
  const int clamped_end = std::clamp(start_x + 8 * x_sign, 0, frontend::GbaWidth);
  const int dx_min  = x_sign * (clamped_start - start_x);
  const int dx_max  = x_sign * (clamped_end   - start_x);

  for (int dx = dx_min, screen_x = clamped_start; dx < dx_max; dx++, screen_x += x_sign) {
    if (dest[screen_x].IsFilled()) continue;

    u8 vram_entry = tile_line_base[dx >> 1];
    u8 palette_nibble = (dx & 1) ? (vram_entry >> 4) : (vram_entry & 0xf);

    // todo: check window
    if (palette_nibble) {
      dest[screen_x].SetColor(((vu16*)palette_base)[palette_nibble], blend_top, blend_bottom);
    }
  }
}

static inline void Render8bpp(Pixel* dest, bool blend_top, bool blend_bottom, int start_x, const int x_sign, u8* tile_line_base, u8* palette_base) {
  const int clamped_start = std::clamp(start_x, 0, frontend::GbaWidth);
  const int clamped_end = std::clamp(start_x + 8 * x_sign, 0, frontend::GbaWidth);
  const int dx_min  = x_sign * (clamped_start - start_x);
  const int dx_max  = x_sign * (clamped_end   - start_x);

  for (int dx = dx_min, screen_x = clamped_start; dx < dx_max; dx++, screen_x += x_sign) {
    if (dest[screen_x].IsFilled()) continue;

    u8 vram_entry = tile_line_base[dx];

    // todo: check window
    if (vram_entry) {
      dest[screen_x].SetColor(((vu16*)palette_base)[vram_entry], blend_top, blend_bottom);
    }
  }
}

static inline void RenderRegularScanline(u32 bg, u32 scanline, Pixel* dest) {
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

      Render4bpp(
          dest,
          bg_data.blend_top,
          bg_data.blend_bottom,
          start_x,
          x_sign,
          &mem_vram[address],
          &mem_pltt[palette_bank * 0x20]
      );
    }
    else {
      // 8bpp
      address += tile_id * 0x40;  // beginning of tile
      address += dy * 8;          // beginning of tile sliver

      Render8bpp(
          dest,
          bg_data.blend_top,
          bg_data.blend_bottom,
          start_x,
          x_sign,
          &mem_vram[address],
          mem_pltt
      );
    }
  }
}

static inline void SetAffinePixel(Pixel& pixel, const BGData& bg_data, u8 tile_id, u32 cbb, u8 dx, u8 dy) {
  u32 address = (cbb * 0x4000) | (tile_id * 0x40) | (dy * 8) | dx;
  u8 vram_entry = mem_vram[address];

  if (vram_entry) {
    pixel.SetColor(((vu16*)mem_pltt)[vram_entry], bg_data.blend_top, bg_data.blend_bottom);
  }
}

static inline void RenderAffineScanline(u32 bg, u32 scanline, Pixel* dest) {
  static constexpr u16 AffineSizeTable[] = { 128, 256, 512, 1024 };

  if (!(REG_DISPCNT & (0x0100 << bg))) {
    // disabled in dispcnt
    return;
  }

  auto bg_data = GetBGData(bg);

  // update reference point (happens every scanline)
  bg_data.rot_scale.ref_x += scanline * bg_data.rot_scale.pb;
  bg_data.rot_scale.ref_y += scanline * bg_data.rot_scale.pd;

  u32 char_base_block   = (bg_data.bgcnt >> 2) & 3;
  u32 color_mode        = (bg_data.bgcnt >> 7) & 1;
  u32 screen_base_block = (bg_data.bgcnt >> 8) & 0x1f;
  u32 screen_size       = (bg_data.bgcnt >> 14) & 3;
  bool wraparound       = ((bg_data.bgcnt >> 13) & 1) != 0;

  u32 bg_size           = AffineSizeTable[screen_size];

  for (int i = 0; i < frontend::GbaWidth; i++) {
    s32 screen_entry_x = (bg_data.rot_scale.ref_x + bg_data.rot_scale.pa * i) >> 8;  // fractional part
    s32 screen_entry_y = (bg_data.rot_scale.ref_y + bg_data.rot_scale.pc * i) >> 8;  // fractional part

    if ((std::clamp<s32>(screen_entry_x, 0, bg_size) != screen_entry_x) ||
        (std::clamp<s32>(screen_entry_y, 0, bg_size) != screen_entry_y)) {
      if (wraparound) {
        screen_entry_x &= bg_size - 1;
        screen_entry_y &= bg_size - 1;
      }
      else {
        continue;
      }
    }

    u32 screen_entry_index = (screen_base_block * 0x800) | ((screen_entry_y >> 3) * (bg_size >> 3)) | (screen_entry_x >> 3);
    u8 screen_entry = mem_vram[screen_entry_index];

    SetAffinePixel(dest[i], bg_data, screen_entry, char_base_block, screen_entry_x & 7, screen_entry_y & 7);
  }
}

static_assert(sizeof(struct OamData) == 8, "Incorrect OamData size for use");
static inline void RenderRegularObject(const struct OamData& obj, u32 scanline, Pixel* dest, bool obj_1d_mapping) {
  s32 start_x = (s32)(obj.x << 23) >> 23;
  auto size = ObjSizeTable[obj.shape][obj.size];
  s16 obj_y = obj.y;
  if (obj_y > frontend::GbaHeight) obj_y -= 0x100;
  s32 dy = scanline - obj_y;
  if (obj.matrixNum & (1 << 4)) {
    // yflip
    dy = size.height - dy - 1;
  }

  s32 x_sign = 1;
  if (obj.matrixNum & (1 << 3)) {
    // hflip
    x_sign = -1;
    start_x += size.width - 1;
  }

  // offset of tile
  u32 sliver_base_address = obj.tileNum * 0x20;
  u32 sliver_size = obj.bpp ? 8 : 4;
  sliver_base_address += obj_1d_mapping ? size.width * (dy >> 3) * sliver_size : (32 * 0x20 * (dy >> 3));
  // within tile
  sliver_base_address += sliver_size * (dy & 7);

  // VRAM masking for OBJ start
  sliver_base_address = 0x1'0000 | (sliver_base_address & 0x7fff);

  if (obj.bpp) {
    for (int tile_x = 0; tile_x < size.width >> 3; tile_x++) {
      Render8bpp(
          dest,
          false,  // todo
          false,  // todo
          start_x + 8 * tile_x * x_sign,
          x_sign,
          &mem_vram[sliver_base_address + 0x40 * tile_x],
          &mem_pltt[0x200]
      );
    }
  }
  else {
    for (int tile_x = 0; tile_x < size.width >> 3; tile_x++) {
      Render4bpp(
          dest,
          false,  // todo
          false,  // todo
          start_x + 8 * tile_x * x_sign,
          x_sign,
          &mem_vram[sliver_base_address + 0x20 * tile_x],
          &mem_pltt[0x200 + 0x20 * obj.paletteNum]
      );
    }
  }
}

static inline void SetAffineObjPixel(const struct OamData& obj, Pixel* dest, const ObjSize& size, u32 px, u32 py, bool obj_1d_mapping) {
  // start of object vram
  u32 pixel_address = 0x10000;
  pixel_address += obj.tileNum * 0x20;
  pixel_address += obj_1d_mapping ? (size.width * (py >> 3) * 4) : (32 * 0x20 * (py >> 3));

  if (obj.bpp) {
    // 8bpp
    pixel_address += 8 * (py & 7);
    pixel_address += 0x40 * (px >> 3);

    u8 vram_entry = mem_vram[pixel_address + (px & 7)];
    if (vram_entry) {
      // todo: blend mode
      dest->SetColor(((vu16*)mem_pltt)[0x100 + vram_entry], false, false);
    }
  }
  else {
    // 4bpp
    pixel_address += 4 * (py & 7);
    pixel_address += 0x20 * (px >> 3);

    u8 palette_nibble = mem_vram[pixel_address + ((px & 7) >> 1)];
    if (px & 1) {
      palette_nibble >>= 4;
    }
    palette_nibble &= 0xf;

    if (palette_nibble) {
      // todo: blend mode
      dest->SetColor(((vu16*)mem_pltt)[0x100 + obj.paletteNum * 0x10 + palette_nibble], false, false);
    }
  }
}

static inline void RenderAffineObject(const struct OamData& obj, u32 scanline, Pixel* dest, bool obj_1d_mapping) {
  s32 start_x = (s32)(obj.x << 23) >> 23;
  s16 obj_y = obj.y;
  if (obj_y > frontend::GbaHeight) obj_y -= 0x100;

  auto size = ObjSizeTable[obj.shape][obj.size];
  s16 pa, pb, pc, pd;
  pa = (s16)((struct OamData*)mem_oam)[4 * obj.matrixNum + 0].affineParam;
  pb = (s16)((struct OamData*)mem_oam)[4 * obj.matrixNum + 1].affineParam;
  pc = (s16)((struct OamData*)mem_oam)[4 * obj.matrixNum + 2].affineParam;
  pd = (s16)((struct OamData*)mem_oam)[4 * obj.matrixNum + 3].affineParam;

  u32 px0 = size.width >> 1;
  u32 py0 = size.height >> 1;

  s16 dx, dy;
  u32 fictional_width;

  if (obj.affineMode == 0b11) {
    // double rendering
    dx = -size.width;
    dy = scanline - obj_y - size.height;
    fictional_width = size.width << 1;
  }
  else {
    dx = -size.width >> 1;
    dy = scanline - obj_y - (size.height >> 1);
    fictional_width = size.width;
  }

  const int ix_min = std::max(-start_x, 0);
  const int ix_max = std::min<int>(start_x + fictional_width, frontend::GbaWidth) - start_x;
  for (int ix = ix_min; ix < ix_max; ix++) {
    if (dest[start_x + ix].IsFilled()) continue;

    // todo: check window
    // transform
    u32 px = ((pa * (dx + ix) + pb * dy) >> 8) + px0;
    u32 py = ((pc * (dx + ix) + pd * dy) >> 8) + py0;

    // use actual width of sprite, even for double rendering
    if (px >= size.width || py >= size.height) continue;

    // todo: object window
    SetAffineObjPixel(obj, &dest[start_x + ix], size, px, py, obj_1d_mapping);
  }
}

static inline std::vector<struct OamData> GetObjects(u32 scanline) {
  const u16 dispcnt = REG_DISPCNT;
  if (!(dispcnt & DISPCNT_OBJ_ON)) {
    return {};
  }
  std::vector<std::pair<u32, struct OamData>> oam{};

  for (u16 i = 0; i < 0x80; i++) {
    struct OamData obj = ((struct OamData*)mem_oam)[i];

    if (obj.affineMode == 0b10) continue;  // sprite hidden
    if (obj.objMode    == 0b10) continue;  // object window

    s16 obj_y = obj.y;
    if (obj_y > frontend::GbaHeight) obj_y -= 0x100;
    const ObjSize size = ObjSizeTable[obj.shape][obj.size];
    switch (obj.affineMode) {
      case 0b00:
      case 0b01:
        if (std::clamp<s16>(obj_y, scanline - size.height + 1, scanline) == obj_y)
          oam.emplace_back(i, obj);
        break;
      case 0b11:
        // affine double
        if (std::clamp<s16>(obj_y, scanline - 2 * size.height + 1, scanline) == obj_y)
          oam.emplace_back(i, obj);
        break;
      default:
        log_fatal("Invalid object mode: %x", obj.objMode);
    }
  }

  // sort based on priority, then index
  std::sort(oam.begin(), oam.end(), [](auto& obj_a, auto& obj_b) {
    if (obj_a.second.priority == obj_b.second.priority) {
      return obj_a.first < obj_b.first;
    }
    return obj_a.second.priority < obj_b.second.priority;
  });

  std::vector<struct OamData> result{};
  result.reserve(oam.size());
  std::transform(oam.begin(), oam.end(), std::back_inserter(result), [](const auto& p) { return p.second; });
  return result;
}

static inline void RenderObject(const struct OamData& obj, u32 scanline, Pixel* dest, bool obj_1d_mapping) {
  switch (obj.affineMode) {
    case 0b00:
      RenderRegularObject(obj, scanline, dest, obj_1d_mapping);
      break;
    case 0b01:
      // affine
    case 0b11:
      // affine double
      RenderAffineObject(obj, scanline, dest, obj_1d_mapping);
      break;
  }
}

static inline void ComposeScanline(color_t* dest, Pixel* scanline) {
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

void RenderScanline(u32 scanline, color_t* dest) {
  Pixel pixels[frontend::GbaWidth] = {};

  const u16 dispcnt = REG_DISPCNT;
  u16 mode = dispcnt & 0x3;

  auto objects = GetObjects(scanline);
  auto curr_obj = objects.begin();
  bool obj_1d_mapping = (dispcnt & DISPCNT_OBJ_1D_MAP) != 0;

  // only modes 0 and 1 are used in pokeruby
  // just look for DISPCNT_MODE_x macros, and you will not find any
  // other than 0 and 1
  switch (mode) {
    case 0: {
      // order layers by priority
      std::array<u32, 4>       layers = { 0, 1, 2, 3 };
      const std::array<u32, 4> priorities  = {
          REG_BG0CNT & 3u,
          REG_BG1CNT & 3u,
          REG_BG2CNT & 3u,
          REG_BG3CNT & 3u,
      };

      std::sort(layers.begin(), layers.end(), [&](auto l, auto r) {
        if (priorities[l] == priorities[r]) {
          return l < r;
        }
        return priorities[l] < priorities[r];
      });

      for (const auto& layer : layers) {
        for (; curr_obj != objects.end() && curr_obj->priority <= priorities[layer]; curr_obj++)
          RenderObject(*curr_obj, scanline, pixels, obj_1d_mapping);
        RenderRegularScanline(layer, scanline, pixels);
      }
      break;
    }
    case 1: {
      // order layers by priority
      std::array<u32, 3>       layers = { 0, 1, 2 };
      const std::array<u32, 4> priorities  = {
          REG_BG0CNT & 3u,
          REG_BG1CNT & 3u,
          REG_BG2CNT & 3u,
      };

      std::sort(layers.begin(), layers.end(), [&](auto l, auto r) {
        if (priorities[l] == priorities[r]) {
          return l < r;
        }
        return priorities[l] < priorities[r];
      });

      for (const auto& layer: layers) {
        for (; curr_obj != objects.end() && curr_obj->priority <= priorities[layer]; curr_obj++)
          RenderObject(*curr_obj, scanline, pixels, obj_1d_mapping);
        if (layer == 2) {
          // affine layer
          RenderAffineScanline(2, scanline, pixels);
        }
        else {
          RenderRegularScanline(layer, scanline, pixels);
        }
      }
      break;
    }
    case 2: {
      if ((REG_BG3CNT & 3) < (REG_BG2CNT & 3)) {
        // 3 is only rendered first if its priority is strictly lower
        RenderAffineScanline(3, scanline, pixels);
        RenderAffineScanline(2, scanline, pixels);
      }
      else {
        RenderAffineScanline(2, scanline, pixels);
        RenderAffineScanline(3, scanline, pixels);
      }
      break;
    }
    default: {
      log_fatal("Unimplemented rendering mode: %d", mode);
    }
  }

  ComposeScanline(dest, pixels);
}

}