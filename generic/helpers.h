#pragma once

extern "C" {
#include "gba/gba.h"

extern u8 IORegisters[0x400];
extern u8 TrappedIORegisters[0x400];

extern u8 mem_pltt[0x400];
extern u8 mem_vram[0x18000];
extern u8 mem_oam[0x400];

extern struct SoundInfo* sound_info;

extern vu16 gpio_data;
extern vu16 gpio_direction;
extern vu16 gpio_read_enable;

}

enum class Interrupt : u32 {
  VBlank,
  HBlank,
  VCountMatch,
  Timer0,
  Timer1,
  Timer2,
  Timer3,
  Serial,
  Dma0,
  Dma1,
  Dma2,
  Dma3,
  Keypad,
  Gamepak
};