#include "helpers.h"
#include "frontend.h"
#include "agb_flash_port.h"
#include "ppu.h"
#include "log.h"
#include <SDL.h>

#include <cstdio>

namespace frontend {

#define WINDOW_TITLE "Pokemon Ruby"
#define DO_FRAME_COUNTER

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;
SDL_GameController* controller = nullptr;

enum class KeypadButton : u16 {
  A      = 0x0001,
  B      = 0x0002,
  Select = 0x0004,
  Start  = 0x0008,
  Right  = 0x0010,
  Left   = 0x0020,
  Up     = 0x0040,
  Down   = 0x0080,
  R      = 0x0100,
  L      = 0x0200,
};

u16 Keypad = 0;

u16 Screen[GbaWidth * GbaHeight];

static void InitGamecontroller() {
  if (SDL_NumJoysticks() < 0) {
    printf("No gamepads detected\n");
  }
  else {
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
      if (SDL_IsGameController(i)) {
        controller = SDL_GameControllerOpen(i);

        if (!controller) {
          printf("Failed to connect to gamecontroller at index %d\n", i);
          continue;
        }

        printf("Connected game controller at index %d\n", i);
        return;
      }
    }
  }
  printf("No gamepads detected (only joysticks)\n");
}

u64 FrameCounter = 0;
u64 OldTicks = 0;
char TitleBuffer[200] = {};

void InitFrontend() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER)) {
    log_fatal("Error initializing SDL2: %s", SDL_GetError());
  }

  window = SDL_CreateWindow(
      WINDOW_TITLE,
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      Scale * GbaWidth,
      Scale * GbaHeight,
      SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
  );
  renderer = SDL_CreateRenderer(
      window,
      -1,
      SDL_RENDERER_ACCELERATED // | SDL_RENDERER_PRESENTVSYNC
  );
  texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_ABGR1555,
      SDL_TEXTUREACCESS_STREAMING,
      GbaWidth,
      GbaHeight
  );

  SDL_GL_SetSwapInterval(0);
  InitGamecontroller();

  flash::LoadFlashMemory();
  OldTicks = SDL_GetTicks();
}

void CloseFrontend() {
  SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

  SDL_DestroyWindow(window);
  SDL_Quit();

  flash::DumpFlashMemory();
}

void RunFrame() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        CloseFrontend();
        exit(0);
      case SDL_CONTROLLERBUTTONDOWN: {
        switch (event.cbutton.button) {
          case 0: Keypad |= static_cast<u16>(KeypadButton::A); break;
          case 1: Keypad |= static_cast<u16>(KeypadButton::B); break;
          case 4: Keypad |= static_cast<u16>(KeypadButton::Select); break;
          case 6: Keypad |= static_cast<u16>(KeypadButton::Start); break;
          case 11: Keypad |= static_cast<u16>(KeypadButton::Up); break;
          case 12: Keypad |= static_cast<u16>(KeypadButton::Down); break;
          case 13: Keypad |= static_cast<u16>(KeypadButton::Left); break;
          case 14: Keypad |= static_cast<u16>(KeypadButton::Right); break;
          case 9: Keypad |= static_cast<u16>(KeypadButton::L); break;
          case 10: Keypad |= static_cast<u16>(KeypadButton::R); break;
          default: break;
        }
        break;
      }
      case SDL_CONTROLLERBUTTONUP: {
        switch (event.cbutton.button) {
          case 0: Keypad &= ~static_cast<u16>(KeypadButton::A); break;
          case 1: Keypad &= ~static_cast<u16>(KeypadButton::B); break;
          case 4: Keypad &= ~static_cast<u16>(KeypadButton::Select); break;
          case 6: Keypad &= ~static_cast<u16>(KeypadButton::Start); break;
          case 11: Keypad &= ~static_cast<u16>(KeypadButton::Up); break;
          case 12: Keypad &= ~static_cast<u16>(KeypadButton::Down); break;
          case 13: Keypad &= ~static_cast<u16>(KeypadButton::Left); break;
          case 14: Keypad &= ~static_cast<u16>(KeypadButton::Right); break;
          case 9: Keypad &= ~static_cast<u16>(KeypadButton::L); break;
          case 10: Keypad &= ~static_cast<u16>(KeypadButton::R); break;
          default: break;
        }
        break;
      }
      default:
        break;
    }
  }

  ppu::RenderFrame(Screen);

#ifdef DO_FRAME_COUNTER
  FrameCounter++;
  if (FrameCounter >= 300) {
    u64 ticks = SDL_GetTicks();
    if (ticks - OldTicks >= 1000) {
      float fps = (1000.0 * (double)FrameCounter) / (double)(ticks - OldTicks);
      OldTicks = ticks;
      FrameCounter = 0;
      std::snprintf(TitleBuffer, sizeof(TitleBuffer), WINDOW_TITLE " (%.2f fps)", fps);
      SDL_SetWindowTitle(window, TitleBuffer);
    }
  }
#endif

  SDL_RenderClear(renderer);
  SDL_UpdateTexture(texture, nullptr, (const void *)Screen, sizeof(u16) * GbaWidth);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

}