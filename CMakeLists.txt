cmake_minimum_required(VERSION 3.10)
include(FetchContent)

set(ProjectName 
    wave-equation)

project($(ProjectName))
# Use the C++23 standard.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

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


# sources variable contains a link to all cpp files to compile
set(SOURCES
    src/main.cpp
)
add_executable(wave-equation ${SOURCES})

target_link_libraries(${ProjectName} SDL3::SDL3)
target_link_libraries(${ProjectName} glm::glm)
target_link_libraries(${ProjectName} SDL3_image::SDL3_image)

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)

# Set the directories that should be included in the build command for this target
# when running g++ these will be included as -I/directory/path/
target_include_directories(${ProjectName}
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/tinygltf
)