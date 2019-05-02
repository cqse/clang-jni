#!/bin/bash

# This is the main linux script for building the JNI native part for
# the clang bindings. This is intended to be run on a fresh Ubunutu
# installation (last tested with 18.04), such as a cloud
# machine. Please read the lines/instructions carefully, before
# executing the script. The script should be executed from the main
# directory of the git and will clone LLVM besides it.

# The tag used to compile from
LLVM_TAG=llvmorg-8.0.0 

echo "Installing required tools"
sudo apt update
sudo apt install -y g++ cmake openjdk-8-jdk-headless swig

echo "Cloning LLVM into sibling directory"
(
    cd ..
    git clone https://github.com/llvm/llvm-project.git
    cd llvm-project
    git checkout $LLVM_TAG
)

echo "Preparing JNI code generated via SWIG"
(
    GENERATED_DIR=generated/eu/cqse/clang
    mkdir -p $GENERATED_DIR
    cd ../llvm-project/clang/include/clang-c

    # we need a version of Index.h without time.h, as swig stumbles there
    cat Index.h | grep -v '<time.h>' > Index-without-time.h

    # prepare preprocessed header
    cpp -H -E Index-without-time.h -I.. -o Index.i -D'__attribute__(pa)='

    # run swig to generate the JNI binding
    swig -module clang -c++ -java -package eu.cqse.clang -outdir ../../../../clang-jni/$GENERATED_DIR Index.i

    # we need to rename and patch the generated file to work with the build
    mv Index_wrap.cxx Index_wrap.cpp
    sed -i -e '/#ifndef SWIGJAVA/a #include "Index.h"' Index_wrap.cpp

    # make generated JNI methods available in list of exported functions
    grep -o 'Java.*clangJNI[^\(]*' Index_wrap.cpp >> ../../tools/libclang/libclang.exports
)

echo "Integrating own Java JNI code"
(
    # generate headers for JNI code
    (cd src && javac -h ../../llvm-project/clang/tools/libclang -cp .:../generated eu/cqse/clang/ClangBinding.java)

    # copy our own native code 
    cp native/eu_cqse_clang_ClangBinding.cpp ../llvm-project/clang/tools/libclang
    mv ../llvm-project/clang/include/clang-c/Index_wrap.cpp ../llvm-project/clang/tools/libclang
    
    # make generated JNI methods available in list of exported functions
    cd ../llvm-project/clang/tools/libclang
    grep -o 'Java_eu_cqse_clang_ClangBinding[^\(]*' eu_cqse_clang_ClangBinding.h >> libclang.exports

    # Monkey patching our build steps into existing cmake files
    sed -i -e '/Indexing.cpp/a eu_cqse_clang_ClangBinding.cpp' CMakeLists.txt
    sed -i -e '/Indexing.cpp/a Index_wrap.cpp' CMakeLists.txt
    sed -i -e '/Index_Internal.h/a eu_cqse_clang_ClangBinding.h' CMakeLists.txt
    sed -i -e '/set.LIBS/i include_directories(../../include/clang-c)' CMakeLists.txt
)

PACKAGE=clang-build-package.tar.gz
echo "Packing for usage on other build machines"
(
    cd ..
    tar czf $PACKAGE clang-jni llvm-project
)

echo "Package ready at $PACKAGE"
