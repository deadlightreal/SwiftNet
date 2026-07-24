#!/bin/bash

cmake -DCMAKE_C_COMPILER=clang .
cmake --build . --config RelWithDebInfo --target swiftnet_debug

exit 0
