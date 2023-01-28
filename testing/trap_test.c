#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <windows.h>
#include <assert.h>

#define PAGE_SIZE 0x1000

// https://stackoverflow.com/questions/8004945/how-to-catch-a-memory-write-and-call-function-with-address-of-write

volatile __attribute__((aligned(PAGE_SIZE))) char range[2 * PAGE_SIZE] = {};


void ProtectRange() {
  DWORD dummy;
  assert(!((size_t)range & (PAGE_SIZE - 1))); // Region is not page aligned
  static_assert(!((size_t)sizeof(range) & (PAGE_SIZE - 1)), "size is not page aligned");

  VirtualProtect((LPVOID)range, sizeof(range), PAGE_READONLY, &dummy);
}

int ExceptionFilter(LPEXCEPTION_POINTERS pEp, void (*pMonitorFxn)(LPEXCEPTION_POINTERS, void*)) {
  DWORD dummy;

  switch (pEp->ExceptionRecord->ExceptionCode) {
    case STATUS_ACCESS_VIOLATION: {
      ULONG access_type = pEp->ExceptionRecord->ExceptionInformation[0];
      UINT_PTR address = pEp->ExceptionRecord->ExceptionInformation[1];
      if ((address >= (UINT_PTR)range) && (address < (UINT_PTR)range + sizeof(range))) {
        VirtualProtect((LPVOID)range, sizeof(range), PAGE_READWRITE, &dummy);

        if (access_type == 1) {
          // write access
          printf("write to %x\n", address - (UINT_PTR)range);
        }
        else {
          printf("read from %x\n", address - (UINT_PTR)range);
        }

        // set TF flag
        pEp->ContextRecord->EFlags |= (1 << 8);
        return EXCEPTION_CONTINUE_EXECUTION;
      }
      return EXCEPTION_CONTINUE_SEARCH;
    }
    case STATUS_SINGLE_STEP: {
      VirtualProtect((LPVOID)range, sizeof(range), PAGE_READONLY, &dummy);
      return EXCEPTION_CONTINUE_EXECUTION;
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}


int main() {
  size_t size = (sizeof(range) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

  ProtectRange();

  __try {
    range[69] = 1;
    printf("read %d\n",range[69]);
  }
  __except(ExceptionFilter(GetExceptionInformation(), NULL)) {

  }
//  VirtualFree(protected, size, MEM_RELEASE);
  return 0;
}