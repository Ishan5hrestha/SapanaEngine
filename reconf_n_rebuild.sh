rm -rf build/Linux
cmake -S . -B build/Linux -G Ninja -DCMAKE_BUILD_TYPE=Debug -DDILIGENT_BUILD_SAMPLE_BASE_ONLY=ON
cmake --build build/Linux -j2 --target Tutorial02_Cube