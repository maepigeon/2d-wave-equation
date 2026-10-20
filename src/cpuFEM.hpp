#ifndef CPU_FEM
#define CPU_FEM
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <inttypes.h>
#include <time.h>
#include "sdlGraphicsHelpers.hpp"
#include "IntrinsicDelaunay.hpp"
#include "DiffGeo.hpp"
#include "Mesh.hpp"
namespace cpuFEM {
    std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> makeIcosphere(int subdivisions);
    int cpuFemMain(int width, int height);
    void waveEquationSimStep(float deltaTime, float waveVelocity, std::vector<float>& u0, std::vector<float>& u1, std::vector<float>& u2,  std::vector<std::vector<std::pair<int, float>>>& L, std::vector<float>& M);
}
#endif