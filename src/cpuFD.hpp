#ifndef CPU_FD
#define CPU_FD
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <inttypes.h>
#include <time.h>
#include "sdlGraphicsHelpers.hpp"

int cpuFdMain(int width, int height, float simSpeed);
void waveEquationSimStep(float deltaTime, int width, int height, std::vector<float>* G0, std::vector<float>* G1, std::vector<float>* G2);
inline float waveVelocity = 40.0; // wave propagation velocity

#endif