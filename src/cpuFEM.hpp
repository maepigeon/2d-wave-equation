#ifndef CPU_FEM
#define CPU_FEM
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <inttypes.h>
#include <time.h>
#include "sdlGraphicsHelpers.hpp"

int cpuFemMain(int width, int height);
void waveEquationSimStep(float deltaTime, int width, int height, std::vector<float>& u0, std::vector<float>& u1, std::vector<float>& u2);

#endif