#include "Viewer.hpp"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "CPURender/RenderManager.hpp"
#include "Model.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"

#include <iostream>

Viewer::MeshIndex Viewer::addMesh(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, std::vector<glm::vec2> uvs, std::vector<uint32_t> indices) {
    MeshData newMeshData;
    newMeshData.mesh.vertices.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size(); i++) {
        Mesh::Vertex v;
        v.position = positions[i];
        if (i < normals.size()) {
            v.normal = normals[i];
        }
        if (i < uvs.size()) {
            v.texcoord = uvs[i];
        }
        newMeshData.mesh.vertices.push_back(v);
    }
    newMeshData.mesh.indices = indices;    
    newMeshData.model.addMesh(newMeshData.mesh);
    newMeshData.model.worldTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
    meshes.push_back(newMeshData);
    return meshes.size() - 1;
}


Viewer::MeshData Viewer::createMeshData(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, std::vector<glm::vec2> uvs, std::vector<uint32_t> indices) {
    Viewer::MeshData newMeshData;
    newMeshData.mesh.vertices.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size(); i++) {
        Mesh::Vertex v;
        v.position = positions[i];
        if (i < normals.size()) {
            v.normal = normals[i];
        }
        if (i < uvs.size()) {
            v.texcoord = uvs[i];
        }
        newMeshData.mesh.vertices.push_back(v);
    }
    newMeshData.mesh.indices = indices;    
    newMeshData.model.addMesh(newMeshData.mesh);
    return newMeshData;
}

void Viewer::updateMesh(Viewer::MeshIndex index, MeshData data) {
    meshes[index] = data;
}

// Opens a window and renders the specified mesh
void Viewer::createWindow(int width, int height, float scaleWindow) {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("cpuFEM viewer", width, height, SDL_WINDOW_INPUT_FOCUS);
    SDL_SetWindowRelativeMouseMode(window, true);
    sdlRenderer = SDL_CreateRenderer(window, nullptr);
    Color background = {20, 20, 28, 255};
    renderer = new RenderManager(sdlRenderer, window, background, width, height);
    camera.setYawPitch(0.0f, 0.0f); // at origin looking down -Z
    texture = nullptr;
    // If scale > 1, create a texture to render into at low res, then upscale
    scaledTarget = nullptr;
    if (scaleWindow > 1) {
        scaledTarget = SDL_CreateTexture(
            sdlRenderer,
            SDL_PIXELFORMAT_RGBA8888,
            SDL_TEXTUREACCESS_TARGET,
            width, height
            );
        if (!scaledTarget) {
            std::cerr << "SDL_CreateTexture Error: " << SDL_GetError() << std::endl;
        }
        // Nearest-neighbour (pixels stay crisp when scaled up)
        SDL_SetTextureScaleMode(scaledTarget, SDL_SCALEMODE_NEAREST);
    }
    this->width = width;
    this->height = height;
    aspectRatio = width / (double)height;
    
    quit = false;
    showDepth = false;
    lastTime = SDL_GetTicks();
}
void Viewer::renderFrame(Viewer::MeshIndex meshIndex) {
    uint32_t currentTime = SDL_GetTicks();
    float deltaTime = (currentTime - lastTime) / 1000.0f; // Convert ms to seconds
    lastTime = currentTime;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            quit = true;
            break;
        }
        else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
            if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                quit = true;
                break;
            }
            if (e.key.scancode == SDL_SCANCODE_Z) {
                showDepth = !showDepth;
                //renderer.setDepthView(showDepth); 
            }
        }
        camera.processSDLInputEvent(&e);
    }
    camera.update(deltaTime);
    if (scaledTarget) {
        // Render into the small texture
        SDL_SetRenderTarget(sdlRenderer, scaledTarget);
        renderer->renderModel(&meshes[meshIndex].model, &camera, aspectRatio, texture);

        // Blit small texture to full window, scaled up
        SDL_SetRenderTarget(sdlRenderer, nullptr);
        SDL_FRect dst = { 0, 0, (float)width, (float)height };
        SDL_RenderTexture(sdlRenderer, scaledTarget, nullptr, &dst);
        SDL_RenderPresent(sdlRenderer);
    } else {
        renderer->renderModel(&meshes[meshIndex].model, &camera, aspectRatio, texture);
    }
    

}
void Viewer::closeWindow() {
    if (scaledTarget) {
        SDL_DestroyTexture(scaledTarget);
    }
    renderer->destroy();
    delete renderer;
    renderer = nullptr;
    SDL_DestroyWindow(window);
    SDL_Quit();
}
