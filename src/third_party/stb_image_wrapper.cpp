#include "stb_image_wrapper.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <cstdio>
#include <SDL2/SDL.h>

SDL_Surface* LoadSurfaceFromPNG(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (!data) {
        printf("Failed to load image %s: %s\n", filename, stbi_failure_reason());
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        data,
        width,
        height,
        32,          // bits per pixel
        width * 4,   // pitch
        SDL_PIXELFORMAT_RGBA32
    );

    if (!surface) {
        printf("SDL_CreateRGBSurface failed for %s: %s\n", filename, SDL_GetError());
        stbi_image_free(data);
        return nullptr;
    }

    surface->userdata = data; // store pointer for freeing later
    return surface;
}

void FreeSurface(SDL_Surface* surface) {
    if (!surface) return;
    if (surface->userdata) stbi_image_free((void*)surface->userdata);
    SDL_FreeSurface(surface);
}

