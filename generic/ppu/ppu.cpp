#include "ppu.h"
#include "internal.h"
#include "frontend.h"
#include "helpers.libgba.h"


namespace ppu {


void RenderFrame(color_t* screen) {
  bool hblank_activity = nongeneric::HasHBlankCallback() && (REG_DISPSTAT & DISPSTAT_HBLANK_INTR) && (REG_IE & INTR_FLAG_HBLANK);
  bool vcount_activity = nongeneric::HasVCountCallback() && (REG_DISPSTAT & DISPSTAT_VCOUNT_INTR) && (REG_IE & INTR_FLAG_VCOUNT);
  bool hblank_dma      = false;

  if (hblank_activity || vcount_activity || hblank_dma) {
    log_debug("No one-shot rendering possible");
  }

  // todo: check if gMain.HBlankCallback is set for one shot rendering
  //       or if any DMA starts at HBlank
  for (int i = 0; i < frontend::GbaHeight; i++) {
    RenderScanline(i, screen + i * frontend::GbaWidth);
  }
}

}