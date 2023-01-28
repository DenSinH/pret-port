#include "helpers.h"


extern "C" {

u32 intr_check = 0;
u32 intr_vector = 0;

u8 IORegisters[0x400] = {};
u8 TrappedIORegisters[0x400] = {};

u8 mem_pltt[0x400] = {};
u8 mem_vram[0x18000] = {};
u8 mem_oam[0x400] = {};
u8 mem_ewram[0x40000] = {};
u8 mem_iwram[0x8000] = {};

extern struct SoundInfo* sound_info = nullptr;

vu16 gpio_data = 0;
vu16 gpio_direction = 0;
vu16 gpio_read_enable = 0;

}