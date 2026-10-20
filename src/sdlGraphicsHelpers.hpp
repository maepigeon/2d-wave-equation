#ifndef GRAPHICS_HELPERS
#define GRAPHICS_HELPERS
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <inttypes.h>

typedef union {
    struct {
        std::uint8_t r, g, b, a;
    } channels;
    std::uint32_t bits;
} ColorData;

ColorData hueToColor(float h);
uint8_t float_to_uint8(float value);
void setSurfaceColor(SDL_Surface *surface, int width, int height, ColorData color);
void renderSimTexture(std::vector<float>& G0, SDL_Surface *surface, int width, int height, ColorData color);
#endif