#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <inttypes.h>
#include <time.h>

#include "double.hpp"

typedef union {
    struct {
        std::uint8_t r, g, b, a;
    } channels;
    std::uint32_t bits;
} Color;

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


void waveEquationSimStep(float deltaTime, int width, int height, std::vector<float>& G0, std::vector<float>& G1, std::vector<float>& G2) {
    float a = 40.0; // wave propagation velocity
    float h = 1.0; // spatial distance between elements
    float speed = deltaTime * a / h;
    speed *= speed;

    // https://beltoforion.de/en/recreational_mathematics/2d-wave-equation.php
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            bool isBoundary = x < 1 || y < 1 || x >= width - 1 || y >= height - 1;
            if (isBoundary) {
                G0[y * width + x] = 127;
            } else {
                float laplace = speed * (G1[(y-1) * width + x] + G1[(y+1) * width + x]
                + G1[y * width + x + 1] + G1[y * width + x - 1] - 4 * G1[y * width + x]);
                float timeDerivative_excludingFuture = G2[y * width + x] - 2 * G1[y * width + x];
                float future = laplace - timeDerivative_excludingFuture; 
                G0[y * width + x] = future;   
            }            
        }
    }
}

void renderSimTexture(std::vector<float>& G0, SDL_Surface *surface, int width, int height, Color color) {
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            uint32_t* const target_pixel = (uint32_t *) ((uint8_t *) surface->pixels
                + y * surface->pitch + x * SDL_BYTESPERPIXEL(surface->format));
            uint8_t r = float_to_uint8((G0[y * width + x] / 256. * color.channels.r + G0[y * width + x] ) * 0.5);
            uint8_t g = float_to_uint8((G0[y * width + x] / 256. * color.channels.g + G0[y * width + x] ) * 0.5);
            uint8_t b = float_to_uint8((G0[y * width + x] / 256. * color.channels.b + G0[y * width + x] ) * 0.5);
            Color color = {r, g, b, 255};
            *target_pixel = color.bits;
        }
    }
}

int main(int argc, char* argv[]) {

    Double d;
    d.run_compute();

    // Window setup
    SDL_Init(SDL_INIT_VIDEO);
    int32_t windowWidth = 960;
    int32_t windowHeight = 600;
    int64_t windowFlags = SDL_WINDOW_INPUT_FOCUS;
    SDL_Window* window = SDL_CreateWindow("Renderer", windowWidth, windowHeight, windowFlags);
    SDL_SetWindowRelativeMouseMode(window, true);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // More window setup : Surface, texture, renderer
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(sdlRenderer, 1); 
    SDL_Surface* surface = SDL_CreateSurface(
        windowWidth, windowHeight, SDL_PIXELFORMAT_ABGR8888
    );
    SDL_Texture* texture = SDL_CreateTexture(
            sdlRenderer,
            SDL_PIXELFORMAT_ABGR8888,
            SDL_TEXTUREACCESS_TARGET,
            windowWidth, windowHeight
            );

    // Main loop setup
    SDL_Event e;
    bool quit = false;
    float time = 0;

    // Grids setup
    int gridWidth = windowWidth;
    int gridHeight = windowHeight;
    std::vector<float> buffer0(gridWidth * gridHeight, 127.);
    std::vector<float> buffer1(gridWidth * gridHeight, 127.);
    std::vector<float> buffer2(gridWidth * gridHeight, 127.);
    std::vector<float>* G0 = &buffer0;
    std::vector<float>* G1 = &buffer1;
    std::vector<float>* G2 = &buffer2;

    // Initial disturbance
    (*G1)[gridHeight/2 * gridWidth + gridWidth/2] = 5000.;

    // Main loop
    uint64_t lastTime = SDL_GetTicks();
    while (!quit) {
        //get deltaTime
        uint64_t currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f; // Convert ms to seconds
        deltaTime = std::min(deltaTime, 1.0f / 60.0f);
        lastTime = currentTime;
        time += deltaTime;
        // Check for input
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
            }
        }
        //Set background color (Note: not needed)
        Color backgroundColor = hueToColor(time / 6.);
        //Color backgroundColor = hueToColor(time / 6.);
        //setSurfaceColor(surface, gridWidth, gridHeight, backgroundColor);
       
        // Run the 2d wave equation step
        waveEquationSimStep(deltaTime, gridWidth, gridHeight, *G0, *G1, *G2);
        
        // Cycle grid buffer pointers
        std::vector<float>* tmp = G2;
        G2 = G1;
        G1 = G0;
        G0 = tmp;

        //Render image to surface
        SDL_LockSurface(surface);
            renderSimTexture(*G0, surface, windowWidth, windowHeight, backgroundColor);
        SDL_UnlockSurface(surface);

        // Display the image
        SDL_SetRenderTarget(sdlRenderer, nullptr);
        SDL_FRect dst = { 0, 0, (float)windowWidth, (float)windowHeight };
        SDL_UpdateTexture(texture, nullptr, surface->pixels, surface->pitch);
        SDL_RenderTexture(sdlRenderer, texture, NULL, NULL);
        SDL_RenderPresent(sdlRenderer);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}