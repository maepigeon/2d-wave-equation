#include "sdlGraphicsHelpers.hpp"

uint8_t float_to_uint8(float value) {
    value = std::round(value);
    if (value < 0.0f)   value = 0.0f;
    if (value > 255.0f) value = 255.0f;
    return static_cast<uint8_t>(value);
}

// hue value in range [0, 1.0) -> Bright color w/ S=V=C=1
Color hueToColor(float h) {
    h = std::fmod(h, 1.0f); // take part of value after decimal

    float x = 1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f);
    std::uint8_t X = static_cast<std::uint8_t>(x * 255.0f + 0.5f);  // rounded

    float sectorSize = 1.0f / 6.0f;
    Color c;
    if      (h < 1.f * sectorSize) { c = {255,   X,   0, 255}; }
    else if (h < 2.f * sectorSize) { c = {  X, 255,   0, 255}; }
    else if (h < 3.f * sectorSize) { c = {  0, 255,   X, 255}; }
    else if (h < 4.f * sectorSize) { c = {  0,   X, 255, 255}; }
    else if (h < 5.f * sectorSize) { c = {  X,   0, 255, 255}; }
    else                           { c = {255,   0,   X, 255}; }
    return c;
}

void setSurfaceColor(SDL_Surface *surface, int width, int height, Color color) {
    SDL_LockSurface(surface);
    // Use memset for fast clearing if color is black (0x00000000)
    // For other colors, use fast fill
    uint32_t fill_color = color.bits;
    uint32_t* pixels = (uint32_t*)surface->pixels;
    int pixel_count = width * height;
    
    // memset only works for 0, but we can use it for black background
    if (fill_color == 0) {
        memset(pixels, 0, pixel_count * sizeof(uint32_t));
    } else {
        // Fast fill for any color
        for (int i = 0; i < pixel_count; i++) {
            pixels[i] = fill_color;
        }
    }
    SDL_UnlockSurface(surface);
}
void renderSimTexture(std::vector<float>& G0, SDL_Surface *surface, int width, int height, Color color) {
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            uint32_t* target_pixel = (uint32_t *) ((uint8_t *) surface->pixels + y * surface->pitch + x * SDL_BYTESPERPIXEL(surface->format));
            float value = G0[y * width + x];
            uint8_t r = float_to_uint8((value + color.channels.r) * 0.5);
            uint8_t g = float_to_uint8((value + color.channels.g) * 0.5);
            uint8_t b = float_to_uint8((value + color.channels.b) * 0.5);
            Color c = {r, g, b, 255};
            *target_pixel = c.bits;
        }
    }
}