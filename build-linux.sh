#!/bin/bash

# This is the main linux script for building the JNI native part for
# the clang bindings. This is intended to run on the results of the
# prepare script.

# The number of cores used for compilation
CORES=16

# Customization to allow reuse of script for mac and linux
EXTENSION=linux.so
SOURCE=libclang.so

if [[ "$OSTYPE" == "darwin"* ]]
then
    EXTENSION=mac.dylib
    SOURCE=libclang.dylib
fi


echo "Patching cmake files to work on this machine"
(
    cd ../llvm-project/clang/tools/libclang

    if [[ "$OSTYPE" == "darwin"* ]]
    then
        JDK_HOME=$(/usr/libexec/java_home 2>/dev/null)
        if [ -z "$JDK_HOME" ]; then
            echo "ERROR: No JDK found. Install a JDK and retry."
            exit 1
        fi
        echo "Using JDK at $JDK_HOME"
        gsed -i -e "/set.LIBS/i include_directories($JDK_HOME/include/)" CMakeLists.txt
        gsed -i -e "/set.LIBS/i include_directories($JDK_HOME/include/darwin)" CMakeLists.txt
    else
        # these are the include dirs for OpenJDK 8
        sed -i -e '/set.LIBS/i include_directories(/usr/lib/jvm/java-8-openjdk-amd64/include)' CMakeLists.txt
        sed -i -e '/set.LIBS/i include_directories(/usr/lib/jvm/java-8-openjdk-amd64/include/linux)' CMakeLists.txt
    fi
)

if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Patching macOS export scripts to include JNI symbols"
    (
        cd ../llvm-project/clang/tools/libclang

        # On macOS, the linker uses an export list generated from libclang.map.
        # Add JNI exports so they are included in the dylib.
        python3 -c "
with open('libclang.exports') as f:
    jni = [l.strip() for l in f if l.strip().startswith('Java_')]
with open('libclang.map') as f:
    c = f.read()
c = c.replace('};', '\n'.join(f'    {s};' for s in jni) + '\n};', 1)
with open('libclang.map', 'w') as f:
    f.write(c)
print(f'Added {len(jni)} JNI symbols to libclang.map')
"

        # Patch the export list generator to also match Java_ symbols
        gsed -i "s/(clang_\[/((?:clang_|Java_)[/" linker-script-to-export-list.py
    )
fi

echo "Running cmake"
(
    cd ../llvm-project
    mkdir -p build
    cd build

    if [[ "$OSTYPE" == "darwin"* ]]; then
        cmake -DLLVM_ENABLE_PROJECTS="clang" \
              -DCMAKE_BUILD_TYPE=Release \
              -DLLVM_TARGETS_TO_BUILD=AArch64 \
              -G "Unix Makefiles" ../llvm
    else
        cmake -DLLVM_ENABLE_PROJECTS="clang" \
              -DCMAKE_BUILD_TYPE=Release \
              -DLLVM_TARGETS_TO_BUILD=X86 \
              -G "Unix Makefiles" ../llvm
    fi
)

BUILD_START=$SECONDS
echo "Running make"
(
    cd ../llvm-project/build
    make -j$CORES libclang
)
BUILD_DURATION=$(( SECONDS - BUILD_START ))

echo "Copying resulting library"
(
    cp ../llvm-project/build/lib/$SOURCE libs/libclang-$EXTENSION
)

echo ""
echo "done"
echo "Build time: $(( BUILD_DURATION / 60 ))m $(( BUILD_DURATION % 60 ))s"
echo "Please record this in BUILD-TIMES.md"
