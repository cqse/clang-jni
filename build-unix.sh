#!/bin/bash

set -euo pipefail

# This is the main linux script for building the JNI native part for
# the clang bindings. This is intended to run on the results of the
# prepare script.

# The number of cores used for compilation
CORES=16

# Customization to allow reuse of script for mac and linux
EXTENSION=linux.so
SOURCE=libclang.so
SED=sed
JDK_NAME=java-8-openjdk-amd64

if [[ "$OSTYPE" == "darwin"* ]]
then
    EXTENSION=mac.dylib
    SOURCE=libclang.dylib
    SED=gsed
    JDK_NAME=jdk1.8.0_241.jdk
fi

echo "Patching cmake files to work on this machine"
(
    cd ../llvm-project/clang/tools/libclang

    if [[ "$OSTYPE" == "darwin"* ]]
    then
        # Taken from JDK 11.0.2
        $SED -i -e "/set.LIBS/i include_directories(/Library/Java/JavaVirtualMachines/${JDK_NAME}/Contents/Home/include/)" CMakeLists.txt
        $SED -i -e "/set.LIBS/i include_directories(/Library/Java/JavaVirtualMachines/${JDK_NAME}/Contents/Home/include/darwin)" CMakeLists.txt
    else
        # these are the include dirs for OpenJDK 8
        $SED -i -e "/set.LIBS/i include_directories(/usr/lib/jvm/${JDK_NAME}/include)" CMakeLists.txt
        $SED -i -e "/set.LIBS/i include_directories(/usr/lib/jvm/${JDK_NAME}/include/linux)" CMakeLists.txt
    fi
)

echo "Running cmake"
(
    cd ../llvm-project
    mkdir -p build
    cd build
    cmake -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -G "Unix Makefiles" ../llvm
)

echo "running make"
(
    cd ../llvm-project/build
    make -j$CORES
)

echo "Copying resulting library"
(
    cp ../llvm-project/build/lib/$SOURCE libs/libclang-$EXTENSION
)

echo "done"