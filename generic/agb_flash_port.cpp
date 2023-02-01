#include "agb_flash_port.h"
#include "helpers.h"
extern "C" {
#include "gba/flash_internal.h"
}
#include "log.h"

#include <array>
#include <string>
#include <fstream>

#ifndef NDEBUG
#define CHECK_FLASH_OFFSET(_offset) \
if ((_offset) >= flash::SectorSize) { \
  log_fatal("Out of bounds offset flash write: %x", _offset); \
}

#define CHECK_FLASH_SECTOR(_sectorNum) \
if ((_sectorNum) >= flash::NumSectors) { \
  log_fatal("Out of bounds sector flash write: %d", _sectorNum); \
}
#else
#define CHECK_FLASH_OFFSET(_offset)
#define CHECK_FLASH_SECTOR(_sectorNum)
#endif

extern "C" const u16 mxMaxTime[];

namespace flash {

// same as other flash implementations
static constexpr size_t SectorSize = 4096;
static constexpr size_t NumSectors = 32;

static std::array<std::array<u8, SectorSize>, NumSectors> FlashMemory = {};
static const std::string SavePath = "pokeruby.sav";

static void DumpFlashMemory() {
  std::ofstream file(SavePath, std::ios::trunc | std::ios::binary);
  if (file.is_open()) {
    file.write(reinterpret_cast<const char*>(&FlashMemory[0][0]), SectorSize * NumSectors);
    file.close();
  }
  else {
    log_fatal("Failed to open save file!");
  }
}

void LoadFlashMemory() {
  std::ifstream file(SavePath, std::ios::binary);

  if (file.is_open()) {
    file.read(reinterpret_cast<char*>(&FlashMemory[0][0]), SectorSize * NumSectors);
    file.close();
  }
  else {
    // don't warn, file might not have been created for this game
    // banks have been filled with 1s already on construction
  }
}

u16 ProgramFlashByte(u16 sectorNum, u32 offset, u8 data) {
  CHECK_FLASH_SECTOR(sectorNum)
  CHECK_FLASH_OFFSET(offset)
  FlashMemory[sectorNum][offset] = data;
  DumpFlashMemory();
  return 0;  // always successful
}

u16 ProgramFlashSector(u16 sectorNum, void *src) {
  CHECK_FLASH_SECTOR(sectorNum)
  std::memcpy(FlashMemory[sectorNum].data(), src, SectorSize);
  DumpFlashMemory();
  return 0;  // always successful
}

u16 EraseFlashChip() {
  FlashMemory = {};
  DumpFlashMemory();
  return 0;  // always successful
}

u16 EraseFlashSector(u16 sectorNum) {
  CHECK_FLASH_SECTOR(sectorNum)
  FlashMemory[sectorNum] = {};
  DumpFlashMemory();
  return 0;  // always successful
}

u16 WaitForFlashWrite(u8 phase, u8 *addr, u8 lastData) {
  return 0;  // always successful
}

}

extern "C" {

const struct FlashSetupInfo PortFlash = {
  flash::ProgramFlashByte,
  flash::ProgramFlashSector,
  flash::EraseFlashChip,
  flash::EraseFlashSector,
  flash::WaitForFlashWrite,
  mxMaxTime,
  {
      131072, // ROM size
      {
          flash::SectorSize, // sector size
          12, // bit shift to multiply by sector size (4096 == 1 << 12)
          flash::NumSectors, // number of sectors
          0  // appears to be unused
      },
      { 3, 1 }, // wait state setup data
      { { 0x69, 0x69 } }
  }
};

void HelperFlashWrite(void* addr, u32 data) {
  log_fatal("Unexpected flash write to %p (%08x)", addr, data);
}

void ReadFlash(u16 sectorNum, u32 offset, void *dest, u32 size)
{
  CHECK_FLASH_SECTOR(sectorNum)
  CHECK_FLASH_OFFSET(offset)
  std::memcpy(dest, &flash::FlashMemory[sectorNum][offset], size);
}

u32 VerifyFlashSector_Core(u16 sectorNum, const u8* src, u32 n) {
  CHECK_FLASH_SECTOR(sectorNum)
  CHECK_FLASH_OFFSET(n - 1)

  // todo: we might as well skip this
  for (size_t i = 0; i < flash::SectorSize; i++) {
    if (src[i] != flash::FlashMemory[sectorNum][i]) {
      return i;
    }
  }
  return 0; // return 0 if verified.
}

u32 VerifyFlashSector(u16 sectorNum, u8 *src) {
  return VerifyFlashSector_Core(sectorNum, src, flash::SectorSize);
}

u32 VerifyFlashSectorNBytes(u16 sectorNum, u8 *src, u32 n) {
  return VerifyFlashSector_Core(sectorNum, src, n);
}

}