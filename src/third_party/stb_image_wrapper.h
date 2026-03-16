#pragma once
#include <SDL2/SDL.h>

SDL_Surface* LoadSurfaceFromPNG(const char* filename);
void FreeSurface(SDL_Surface* surface);

