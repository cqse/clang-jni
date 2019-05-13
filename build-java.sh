#!/bin/bash

# This script should be executed at the very last step to compile the
# Java code and package everything in a JAR. This requires the native
# libaries for Linux, Mac and Windows to be placed in the "libs"
# folder.

LINUX_LIB=libclang-linux.so
MAC_LIB=libclang-mac.so
WIN_LIB=libclang-win.dll

NATIVE_LIBS="$LINUX_LIB $MAC_LIB $WIN_LIB"

for lib in $NATIVE_LIBS
do
    if [[ ! -r libs/$lib ]]
    then
        echo "Missing native library $lib in folder libs"
        exit 1   
    fi
done


rm -rf build clang-jni.jar
mkdir -p build
javac -d build -source 1.8 -target 1.8 -cp src:generated `find src generated -name '*.java'`
(cd libs && cp $NATIVE_LIBS ../build/eu/cqse/clang/)

jar -cef eu/cqse/clang/Main clang-jni.jar  -C build eu

echo "done"

