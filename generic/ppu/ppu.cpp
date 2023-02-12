#include "ppu.h"
#include "internal.h"
#include "frontend.h"


namespace ppu {


void RenderFrame(color_t* screen) {
  // todo: check if gMain.HBlankCallback is set for one shot rendering
  //       or if any DMA starts at HBlank
  for (int i = 0; i < frontend::GbaHeight; i++) {
    RenderScanline(i, screen + i * frontend::GbaWidth);
  }
}

}