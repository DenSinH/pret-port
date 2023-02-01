#include "log.h"
#include "helpers.h"
#include "frontend.h"

struct DmaRegister {
  enum class Timing {
    Immediate = 0,
    VBlank = 1,
    HBlank = 2,
    Special = 3,
  };

  u8 dst_addr_ctrl;
  u8 src_addr_ctrl;
  u8 repeat;
  u8 type;
  u8 timing;
  u8 irq;
  u8 enable;
  void* src;
  void* dest;
  u32 count;

  bool ShouldDoTransfer(Timing currentTiming) {
    if (!enable) {
      return false;
    }
    return static_cast<u32>(currentTiming) == timing;
  }

  void DoTransfer(Timing currentTiming) {
    if (!ShouldDoTransfer(currentTiming)) {
      return;
    }

    switch(timing) {
      case 0:  // immediate
        break;
      case 1:  // vblank
        log_warn("Vblank DMA");
        break;
      case 2:  // hblank
        log_fatal("Hblank dma");
      case 3:  // special
        log_fatal("special dma");
    }

    const u32 transfer_size = type ? sizeof(u32) : sizeof(u16);
    s32 dst_increment;
    s32 src_increment;
    switch (dst_addr_ctrl) {
      case 0: dst_increment = transfer_size; break;
      case 1: dst_increment = -transfer_size; break;
      case 2: dst_increment = 0; break;
      case 3: dst_increment = transfer_size; break;
      default: log_fatal("Invalid dst_addr_ctrl: %d", dst_addr_ctrl);
    }
    switch (src_addr_ctrl) {
      case 0: src_increment = transfer_size; break;
      case 1: src_increment = -transfer_size; break;
      case 2: src_increment = 0; break;
      default: log_fatal("Invalid src_addr_ctrl: %d", dst_addr_ctrl);
    }

//    log_info("Transferring %x %dbit units from dma", count, 8 * transfer_size);
    for (u32 i = 0; i < count; i++) {
      memcpy(dest, src, transfer_size);
      dest = (void*)((uintptr_t)dest + dst_increment);
      src  = (void*)((uintptr_t)src  + src_increment);
    }

    src = (void*)((uintptr_t)src + count * src_increment);
    if (dst_addr_ctrl != 3) {
      dest = (void*)((uintptr_t)dest + count * dst_increment);
    }
  }
};

DmaRegister DmaRegisters[4] = {};


extern "C" {

vu8* RegisterAccessIntercept(u32 offset) {
  switch (offset) {
    case REG_OFFSET_VCOUNT: {
      (*(u16*)&TrappedIORegisters[REG_OFFSET_VCOUNT])++;
      (*(u16*)&TrappedIORegisters[REG_OFFSET_VCOUNT]) %= 228;
      break;
    }
    case REG_OFFSET_KEYINPUT: {
      (*(u16*)&TrappedIORegisters[REG_OFFSET_KEYINPUT]) = (~frontend::Keypad) & 0x03ff;
      break;
    }
    case REG_OFFSET_IME:
    case REG_OFFSET_IE:
    case REG_OFFSET_IF:
      break;
    default:
      log_warn("Direct register access at %08x", offset);
      break;
  }

  return (u8*)&TrappedIORegisters[offset];
}

void HelperDmaSet(u32 dmaNum, void* src, void* dest, u32 control) {
  DmaRegisters[dmaNum].count         = control & 0xffff;
  control >>= 16;
  DmaRegisters[dmaNum].dst_addr_ctrl = (control >> 5) & 3;
  DmaRegisters[dmaNum].src_addr_ctrl = (control >> 7) & 3;
  DmaRegisters[dmaNum].repeat        = (control >> 9) & 1;
  DmaRegisters[dmaNum].type          = (control >> 10) & 1;
  DmaRegisters[dmaNum].timing        = (control >> 12) & 3;
  DmaRegisters[dmaNum].irq           = (control >> 14) & 1;
  DmaRegisters[dmaNum].enable        = (control >> 15) & 1;
  DmaRegisters[dmaNum].dest          = dest;
  DmaRegisters[dmaNum].src           = src;

  DmaRegisters[dmaNum].DoTransfer(DmaRegister::Timing::Immediate);
}
//{                                                 \
//    vu32 *dmaRegs = (vu32 *)REG_ADDR_DMA##dmaNum; \
//    dmaRegs[0] = (vu32)(src);                     \
//    dmaRegs[1] = (vu32)(dest);                    \
//    dmaRegs[2] = (vu32)(control);                 \
//    dmaRegs[2];                                   \
//}

void HelperDmaStop(u32 dmaNum) {
//  log_warn("Dma stop");
  DmaRegisters[dmaNum].enable = 0;
  DmaRegisters[dmaNum].timing = 0;
  // todo: dreq
  DmaRegisters[dmaNum].repeat = 0;
}
//{                                                               \
//    vu16 *dmaRegs = (vu16 *)REG_ADDR_DMA##dmaNum;               \
//    dmaRegs[5] &= ~(DMA_START_MASK | DMA_DREQ_ON | DMA_REPEAT); \
//    dmaRegs[5] &= ~DMA_ENABLE;                                  \
//    dmaRegs[5];                                                 \
//}

}