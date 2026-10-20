#include "cpuFD.hpp"

void waveEquationSimStep(float deltaTime, int width, int height, std::vector<float>* G0, std::vector<float>* G1, std::vector<float>* G2) {
    float dx = 1.0; // spatial distance between elements
    float C_MAX = 0.5f; // stability bound
    float courant = waveVelocity * deltaTime / dx;
    uint32_t subSteps = std::max(1, (int)std::ceil(courant / C_MAX));
    float subDt = deltaTime / subSteps;
    float dt2v2_dx2 = (subDt * waveVelocity / dx);
    dt2v2_dx2 *= dt2v2_dx2;

    // https://beltoforion.de/en/recreational_mathematics/2d-wave-equation.php
    for (int i = 0; i < subSteps; i++) {
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                bool isBoundary = x < 1 || y < 1 || x >= width - 1 || y >= height - 1;
                if (isBoundary) {
                    (*G0)[y * width + x] = 127;
                } else {
                    float laplace = dt2v2_dx2 * ((*G1)[(y-1) * width + x] + (*G1)[(y+1) * width + x]
                    + (*G1)[y * width + x + 1] + (*G1)[y * width + x - 1] - 4 * (*G1)[y * width + x]);
                    float timeDerivative_excludingFuture = (*G2)[y * width + x] - 2 * (*G1)[y * width + x];
                    float future = laplace - timeDerivative_excludingFuture; 
                    (*G0)[y * width + x] = future;   
                }            
            }
        }
        std::vector<float>* tmp = G0;
        G0 = G2;
        G2 = G1;
        G1 = tmp;
    }
}

int cpuFdMain(int width, int height, float simSpeed) {
    waveVelocity = simSpeed;
    // Window setup
    SDL_Init(SDL_INIT_VIDEO);
    int64_t windowFlags = SDL_WINDOW_INPUT_FOCUS;
    SDL_Window* window = SDL_CreateWindow("Renderer", width, height, windowFlags);
    SDL_SetWindowRelativeMouseMode(window, true);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    // More window setup : Surface, texture, renderer
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(sdlRenderer, 1); 
    SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ABGR8888);
    SDL_Texture* texture = SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, width, height);
    // Main loop setup
    SDL_Event e;
    bool quit = false;
    float time = 0;

    // Grids setup
    int gridWidth = width;
    int gridHeight = height;
    std::vector<float> buffer0(gridWidth * gridHeight, 127.);
    std::vector<float> buffer1(gridWidth * gridHeight, 127.);
    std::vector<float> buffer2(gridWidth * gridHeight, 127.);
    std::vector<float>* G0 = &buffer0;
    std::vector<float>* G1 = &buffer1;
    std::vector<float>* G2 = &buffer2;

    // Initial disturbance
    //(*G1)[gridHeight/2 * gridWidth + gridWidth/2] = 5000.;
    float cx = gridWidth / 2.0f;
    float cy = gridHeight / 2.0f;
    float sigma = 6.0f; // initial displacment radius
    float amplitude = 60.0f; // peak displacement above rest
    for (uint32_t y = 0; y < gridHeight; y++) {
        for (uint32_t x = 0; x < gridWidth; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float r2 = dx * dx + dy * dy;
            (*G1)[y * gridWidth + x] += amplitude * std::exp(-r2 / (2.0f * sigma * sigma));
        }
    }

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
        ColorData backgroundColor = hueToColor(time / 6.);
        //Color backgroundColor = hueToColor(time / 6.);
        //setSurfaceColor(surface, gridWidth, gridHeight, backgroundColor);
       
        // Run the 2d wave equation step
        waveEquationSimStep(deltaTime, gridWidth, gridHeight, G0, G1, G2);
        
        // Cycle grid buffer pointers
        std::vector<float>* tmp = G2;
        G2 = G1;
        G1 = G0;
        G0 = tmp;

        //Render image to surface
        SDL_LockSurface(surface);
        renderSimTexture(*G0, surface, width, height, backgroundColor);
        SDL_UnlockSurface(surface);

        // Display the image
        SDL_SetRenderTarget(sdlRenderer, nullptr);
        SDL_FRect dst = { 0, 0, (float)width, (float)height };
        SDL_UpdateTexture(texture, nullptr, surface->pixels, surface->pitch);
        SDL_RenderTexture(sdlRenderer, texture, NULL, NULL);
        SDL_RenderPresent(sdlRenderer);
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
