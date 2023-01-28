#include "helpers.libgba.h"

extern "C" {

#include "gba/gba.h"

typedef void (* IntrFunc)();
extern IntrFunc gIntrTable[];

}

namespace nongeneric {

void HandleInterrupt(Interrupt interrupt) {
  // todo: proper interrupt checking...
//  REG_IF |= 1 << static_cast<u32>(interrupt);

  if (!REG_IME) {
    return;
  }

  if (!(REG_IE & (1 << static_cast<u32>(interrupt)))) {
    return;
  }

  IntrFunc intrFunc = nullptr;

  switch(interrupt) {
    case Interrupt::VBlank: intrFunc = gIntrTable[3]; break;
    case Interrupt::HBlank: intrFunc = gIntrTable[2]; break;
    case Interrupt::VCountMatch: intrFunc = gIntrTable[4]; break;
    case Interrupt::Timer0: intrFunc = gIntrTable[5]; break;
    case Interrupt::Timer1: intrFunc = gIntrTable[6]; break;
    case Interrupt::Timer2: intrFunc = gIntrTable[7]; break;
    case Interrupt::Timer3: intrFunc = gIntrTable[1]; break;
    case Interrupt::Serial: intrFunc = gIntrTable[0]; break;
    case Interrupt::Dma0: intrFunc = gIntrTable[8]; break;
    case Interrupt::Dma1: intrFunc = gIntrTable[9]; break;
    case Interrupt::Dma2: intrFunc = gIntrTable[10]; break;
    case Interrupt::Dma3: intrFunc = gIntrTable[11]; break;
    case Interrupt::Keypad: intrFunc = gIntrTable[12]; break;
    case Interrupt::Gamepak: intrFunc = gIntrTable[13]; break;
  }

  if (intrFunc) {
    intrFunc();
  }
}

}
