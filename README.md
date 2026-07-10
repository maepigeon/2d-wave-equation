Demo: https://youtu.be/GHSqBHcc-z4

To build and run

    mkdir build

    cd build

    cmake ../CMakeLists.txt

    cmake --build

    make

    ./wave-equation


To debug

    cmake  ../CMakeLists.txt -B build -DCMAKE_BUILD_TYPE=Debug

    cmake --build build

    lldb ./wave-equation

    run

Note: This has 2 dependencies, which the project discovers using its CMakeLists.txt. SDL3 and glm must first be installed to build.
