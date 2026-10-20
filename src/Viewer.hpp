#ifndef VIEWER
#define VIEWER
#include <vector>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include "Mesh.hpp"
#include "Model.hpp"
#include "Camera.hpp" 
#include "RenderManager.hpp" 
class Viewer {
    
    public:
        struct MeshData {
            std::vector<float> scalars;
            Mesh mesh;
            Model model;
        };
        int typedef MeshIndex;
        MeshIndex addMesh(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, std::vector<glm::vec2> uvs, std::vector<uint32_t> indices);
        MeshData createMeshData(std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals, std::vector<glm::vec2> uvs, std::vector<uint32_t> indices);
        void updateMesh(Viewer::MeshIndex index, MeshData data);
        // Opens a window and renders the specified mesh
        void createWindow(int width, int height, float scaleWindow);
        void renderFrame(MeshIndex meshIndex);
        void closeWindow();
        bool quit;
    private:
        std::vector<MeshData> meshes;
        bool showDepth;
        SDL_Window* window;
        SDL_Renderer* sdlRenderer;
        SDL_Event e;
        uint32_t lastTime;
        double aspectRatio;
        uint16_t width;
        uint16_t height;
        SDL_Surface* texture;
        Camera camera;
        RenderManager* renderer;
        SDL_Texture* scaledTarget;
};
#endif