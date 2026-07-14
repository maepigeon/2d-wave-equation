To build and run

    mkdir build

    cd build

    cmake ../CMakeLists.txt

    cmake .

    make

    ./wave-equation


To compile shaders

    glslc double.comp -o double.comp.spv
    

To debug

    cmake  ../CMakeLists.txt -B build -DCMAKE_BUILD_TYPE=Debug

    cmake --build build

    lldb ./wave-equation

    run

Note: This has 3 dependencies, which the project discovers using its CMakeLists.txt. SDL3, glm, and vkBootstrap must first be installed to build.
