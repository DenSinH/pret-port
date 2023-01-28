#include "helpers.h"
#include "frontend.h"
#include "ppu.h"
#include "log.h"
#include <SDL.h>

namespace frontend {

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
SDL_Texture* texture = nullptr;

u16 Screen[GbaWidth * GbaHeight];

void InitFrontend() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER)) {
    log_fatal("Error initializing SDL2: %s", SDL_GetError());
  }

  window = SDL_CreateWindow(
      "Pokemon Ruby",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      Scale * GbaWidth,
      Scale * GbaHeight,
      SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
  );
  renderer = SDL_CreateRenderer(
      window,
      -1,
      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
  );
  texture = SDL_CreateTexture(
      renderer,
      SDL_PIXELFORMAT_ARGB1555,
      SDL_TEXTUREACCESS_STREAMING,
      GbaWidth,
      GbaHeight
  );
}

void CloseFrontend() {
  SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);

  SDL_DestroyWindow(window);
  SDL_Quit();
}

void RunFrame() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        CloseFrontend();
        exit(0);
      default:
        break;
    }
  }

  ppu::RenderFrame(Screen);

  SDL_RenderClear(renderer);
  SDL_UpdateTexture(texture, nullptr, (const void *)Screen, sizeof(u16) * GbaWidth);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

}