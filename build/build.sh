rm -f CMakeCache.txt
rm -rf CMakeFiles
rm -rf cmake
rm -f Makefile
rm -f cmake_install.cmake

cmake ../src \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_FLAGS="-Wall -Wextra -Wpedantic -Werror -fanalyzer -Wno-analyzer-use-of-uninitialized-value" \
    -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -fanalyzer -Wno-analyzer-use-of-uninitialized-value" \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -B .
make -B -j8
