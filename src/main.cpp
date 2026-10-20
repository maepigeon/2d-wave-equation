#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <inttypes.h>
#include <time.h>
#include "cpuFD.hpp"
#include "wave-gpu.hpp"
#include "double.hpp"
#include "cpuFEM.hpp"

enum programOption {cpuFd, gpuFd, gpuTest, cpuFem, nullMode};

void displayHelp() {
    std::cout << "Execution failed..." << std::endl << std::endl;
    std::cout << "Usage: wave-equation mode[String] width[Int] height[Int] simSpeed[Int]" << std::endl;
    std::cout << "Modes : element of {cpuFd, gpuFd, gpuTest, cpuFem}" << std::endl;
}

int main(int argc, char* argv[]) {
    int width = 800;
    int height = 600;
    int simSpeed = 30;
    programOption mode = nullMode;
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "gpuFd") {
            mode = gpuFd;
        } else if (arg1 == "gpuTest") {
            mode = gpuTest;
        } else if(arg1 == "cpuFd") {
            mode = cpuFd;
        } else if(arg1 == "cpuFem") {
            mode = cpuFem;
        }
        if (argc > 3) {
            width = std::stoi(argv[2]);
            height = std::stoi(argv[3]);
        }
        if (argc > 4) {
            simSpeed = std::stoi(argv[4]);
        }
    }
    if (mode == gpuTest) {
        Double d;
        d.run_compute(100);
    } else if (mode == cpuFem) {
        return cpuFEM::cpuFemMain( width, height);
    } else if (mode == cpuFd) {
        return cpuFdMain(width, height, simSpeed);
    } else if (mode == gpuFd) {
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
        // More window setup: surface, texture, renderer
        SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, nullptr);
        SDL_SetRenderVSync(sdlRenderer, 1); 
        SDL_Surface* surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ABGR8888);
        SDL_Texture* texture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_ABGR8888,SDL_TEXTUREACCESS_TARGET,width, height);
        WaveGpu wavesim;
        wavesim.run_compute(width, height, 1, simSpeed, surface, texture, sdlRenderer);
    } else {
        displayHelp();
    }

    return 0;
}