cmake_minimum_required(VERSION 3.10)
include(FetchContent)

set(ProjectName 
    wave-equation)

project(${ProjectName})
# Use the C++23 standard.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#SDL3: use system install if available, otherwise fetch from source
find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
    message(STATUS "SDL3 not found on system, fetching from source...")
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.2.14
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL3)
endif()
#glm: use system install if available, otherwise fetch from source
find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
    message(STATUS "glm not found on system, fetching from source...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glm)
endif()
find_package(SDL3_image QUIET)
if(NOT SDL3_image_FOUND)
    FetchContent_Declare(
        SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG        release-3.2.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL3_image)
endif()

# Vulkan loader + headers (VMA and vk-bootstrap call into the loader directly).
find_package(Vulkan REQUIRED)

find_package(fetch_vk_bootstrap QUIET)
if(NOT fetch_vk_bootstrap_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        fetch_vk_bootstrap
        GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
        GIT_TAG        v1.3.302
    )
    FetchContent_MakeAvailable(fetch_vk_bootstrap)
endif()

find_package(fetch_vma QUIET)
if(NOT fetch_vma_FOUND)
    FetchContent_Declare(
        fetch_vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        GIT_TAG        v3.1.0
    )
    FetchContent_MakeAvailable(fetch_vma)
endif()

FetchContent_Declare(
    cpu-rasterizer
    GIT_REPOSITORY https://github.com/maepigeon/cpu-rasterizer.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(cpu-rasterizer)


FetchContent_Declare(
    tiny-geolib
    GIT_REPOSITORY https://github.com/maepigeon/tiny-geolib.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(tiny-geolib)

# Rasterizer viewer connection
add_library(viewer STATIC src/Viewer.cpp)
target_link_libraries(viewer PUBLIC glm::glm PRIVATE rasterizer)
target_include_directories(viewer PUBLIC ${cpu-rasterizer_SOURCE_DIR}/src)
target_include_directories(viewer PUBLIC ${cpu-rasterizer_SOURCE_DIR}/src/CPURender)
target_compile_features(viewer PUBLIC cxx_std_23)

# sources variable contains a link to all cpp files to compile
set(SOURCES
    src/main.cpp
    src/sdlGraphicsHelpers.cpp
    src/cpuFD.cpp
    src/wave-gpu.cpp
    src/double.cpp
    src/vma_impl.cpp
    src/cpuFEM.cpp
)

add_executable(wave-equation ${SOURCES})
target_link_libraries(${ProjectName} SDL3::SDL3)
target_link_libraries(${ProjectName} glm::glm)
target_link_libraries(${ProjectName} SDL3_image::SDL3_image)
target_link_libraries(${ProjectName} vk-bootstrap::vk-bootstrap)
target_link_libraries(${ProjectName} GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${ProjectName} Vulkan::Vulkan)
target_link_libraries(${ProjectName} tiny-geolib) 
target_link_libraries(${ProjectName} viewer)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

# Set the directories that should be included in the build command for this target
# when running g++ these will be included as -I/directory/path/
target_include_directories(${ProjectName}
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/tinygltf
)