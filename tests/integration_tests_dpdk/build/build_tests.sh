rm -f CMakeCache.txt
rm -rf CMakeFiles
rm -rf cmake
rm -f Makefile
rm -f cmake_install.cmake

cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=none -B .
make -B -j8
