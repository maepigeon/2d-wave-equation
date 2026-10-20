#include "cpuFEM.hpp"
#include <vector>
#include "Viewer.hpp"
#include "Mesh.hpp"

#include <numeric>
#include <map>
#include <algorithm>
#include <time.h>


namespace cpuFEM {
    int getMidpoint(int a, int b, std::vector<glm::vec3>& vertices, std::map<uint64_t, int>& midpointCache) {
        int min = a;
        int max = b;
        if (max < min) {
            std::swap(min, max);
        }
        uint64_t key = ((uint64_t)max << 32) + (uint64_t)min;
        if (midpointCache.count(key)) {
            return midpointCache[key];
        }
        vertices.push_back(glm::normalize(vertices[a] + vertices[b]));
        midpointCache[key] = vertices.size() - 1;
        return vertices.size() - 1;
    }
    std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> makeIcosphere(int subdivisions) {
        float goldenRatio = (1.f + sqrt(5.f)) / 2.f;
        std::vector<glm::vec3> vertices = {
            glm::normalize(glm::vec3{0, 1, goldenRatio}), glm::normalize(glm::vec3{0, -1, goldenRatio}),
            glm::normalize(glm::vec3{0, 1, -goldenRatio}), glm::normalize(glm::vec3{0, -1, -goldenRatio}),
            glm::normalize(glm::vec3{1, goldenRatio, 0}), glm::normalize(glm::vec3{-1, goldenRatio, 0}),
            glm::normalize(glm::vec3{1, -goldenRatio, 0}), glm::normalize(glm::vec3{-1, -goldenRatio, 0}),
            glm::normalize(glm::vec3{goldenRatio, 0, 1}), glm::normalize(glm::vec3{goldenRatio, 0, -1}),
            glm::normalize(glm::vec3{-goldenRatio, 0, 1}), glm::normalize(glm::vec3{-goldenRatio, 0, -1})
        };
        std::vector<glm::ivec3> faces = {
            {0, 1, 8}, {0,10, 1}, {0, 4, 5}, {0, 8, 4}, {0, 5,10},
            {1, 7, 6}, {1, 6, 8}, {1, 10, 7}, {2, 9, 3}, {2, 3,11},
            {2, 5, 4}, {2, 4, 9}, {2, 11, 5}, {3, 6, 7}, {3, 9, 6},
            {3, 7,11}, {4, 8, 9}, {5, 11,10}, {6, 9, 8}, {7,10,11}
        };
        for (int subdivision = 0; subdivision < subdivisions; subdivision++) {
            std::vector<glm::ivec3> newFaces;
            newFaces.reserve(faces.size() * 4);
            std::map<uint64_t, int> newVertices;
            for (glm::ivec3 f : faces) {
                int xy = getMidpoint(f.x, f.y, vertices, newVertices);
                int yz = getMidpoint(f.y, f.z, vertices, newVertices);
                int zx = getMidpoint(f.z, f.x, vertices, newVertices);

                newFaces.push_back({f.x, xy, zx}); // corner x
                newFaces.push_back({xy, f.y, yz}); // corner y
                newFaces.push_back({zx, yz, f.z}); // corner z
                newFaces.push_back({xy, yz, zx}); // center
            }
            faces.swap(newFaces); 
        }
        return std::pair(vertices, faces);
    }

    void waveEquationSimStep(float deltaTime, float waveVelocity, std::vector<float>& u0, std::vector<float>& u1, std::vector<float>& u2, std::vector<std::vector<std::pair<int, float>>>& L, std::vector<float>& M) {
        float dt2v2 = deltaTime * deltaTime * waveVelocity * waveVelocity;
        for (int i = 0; i < u0.size(); i++) {
            float laplace = 0;
            for (int j = 0; j < L[i].size(); j++) {
                std::pair<int, float> connectedVertex = L[i][j];
                laplace -= connectedVertex.second * ( u1[connectedVertex.first] - u1[i]); 
            }
            laplace /= M[i];
            float timeDerivative_excludingFuture = u2[i] - 2 * u1[i];
            float future = dt2v2 * laplace - timeDerivative_excludingFuture; 
            u0[i] = future;

            // dissipation term
            u0[i] = u0[i] - 0.01f * (u0[i]);
        }
    }

    int cpuFemMain(int width, int height) {
        std::pair<std::vector<glm::vec3>, std::vector<glm::ivec3>> icoSphere = makeIcosphere(4);
        tinyGeo::Mesh sphere = tinyGeo::meshFromArrays(icoSphere.first, icoSphere.second);
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> uvs;
        positions.reserve(sphere.vertices.size());
        normals.reserve(sphere.vertices.size());
        uvs.reserve(sphere.vertices.size());
        for (tinyGeo::Vertex& v : sphere.vertices) {
            positions.push_back(v.position);
            normals.push_back(v.normal);
            uvs.push_back(v.uv);
        }
        Viewer viewer;
        viewer.createWindow(width, height, 2.0f);
        Viewer::MeshIndex index = viewer.addMesh(positions, normals, uvs, sphere.indices);

        tinyGeo::HalfEdgeMesh heMesh = tinyGeo::buildHalfEdgeMesh(sphere);
        tinyGeo::makeIntrinsicDelaunay(heMesh);
        tinyGeo::calculateEdgeLengths(heMesh);   
        std::vector<std::vector<std::pair<int, float>>> L = tinyGeo::cotanLaplacian(heMesh);
        std::vector<float> M = tinyGeo::vertexMasses(heMesh);
        std::vector<float> buffer0(positions.size(), 0.f);
        std::vector<float> buffer1(positions.size(), 0.f);
        std::vector<float> buffer2(positions.size(), 0.f);

        std::vector<float>* u0 = &buffer0;
        std::vector<float>* u1 = &buffer1;
        std::vector<float>* u2 = &buffer2;

        // initial test disturbance
        int disturbanceStartVertex = 1600;
        for (std::pair<int, float> v : (L[disturbanceStartVertex])) {
            (*u1)[v.first] = 0.07f;
            for (std::pair<int, float> v2 : (L[v.first])) {
                (*u1)[v2.first] = 0.07f;
            }
        }
        uint32_t lastTime = SDL_GetTicks();
        while (!viewer.quit) {
            uint32_t currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f; // Convert ms to seconds
            lastTime = currentTime;
            waveEquationSimStep(deltaTime, 0.25f, *u0, *u1, *u2, L, M);
            std::vector<glm::vec3> displacedPositions((*u0).size());
            for (int i = 0; i < (*u0).size(); i++) {
                displacedPositions[i] = positions[i] + glm::length(positions[i]) * (*u0)[i] * glm::normalize(positions[i]) ;
            }
            Viewer::MeshData newMeshData = viewer.createMeshData(displacedPositions, normals, uvs, sphere.indices);
            newMeshData.model.worldTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
            viewer.updateMesh(index, newMeshData);
            viewer.renderFrame(index);
            std::vector<float>* tmp = u2;
            u2 = u1;
            u1 = u0;
            u0 = tmp;
        }
        viewer.closeWindow();
        return 0;
    }
}