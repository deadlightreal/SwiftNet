../build/build_for_testing.sh
cmake . -DCMAKE_BUILD_TYPE=Debug -DSANITIZER=thread
make -B -j8
