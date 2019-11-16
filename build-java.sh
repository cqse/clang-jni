#!/bin/bash

# This script should be executed at the very last step to compile the
# Java code and package everything in a JAR. This requires the native
# libaries for Linux, Mac and Windows to be placed in the "libs"
# folder.

LINUX_LIB=libclang-linux.so
MAC_LIB=libclang-mac.dylib
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

(
    mkdir build/eu/cqse/clang/doc
    cd ../llvm-project/clang-tools-extra/docs-out/clang-tidy/checks
    for i in *.html
    do
        if [[ $i != list.html ]]
        then
            # we extract the main content without the heading
            # (as this just repeats the check name)
            cat $i | hxnormalize -x | hxselect -c 'div.content div.section' \
                | grep -v '<h1>' > ../../../../../clang-jni/build/eu/cqse/clang/doc/$i
        fi
    done
)

jar -cef eu/cqse/clang/Main clang-jni.jar  -C build eu

echo "done"

