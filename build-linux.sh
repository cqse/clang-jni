#!/bin/bash

# This is the main linux script for building the JNI native part for
# the clang bindings. This is intended to be run on a fresh Ubunutu
# installation (last tested with 18.04), such as a cloud
# machine. Please read the lines/instructions carefully, before
# executing the script. The script should be executed from the main
# directory of the git and will clone LLVM besides it.

# The tag used to compile from
LLVM_TAG=llvmorg-8.0.0 

# The number of cores used for compilation
CORES=16


# Install required tools
sudo apt update
sudo apt install -y g++ cmake openjdk-8-jdk-headless swig

# clone LLVM into sibling directory
(
    cd ..
    git clone https://github.com/llvm/llvm-project.git
    cd llvm-project
    git checkout $LLVM_TAG
)

# prepare JNI code generated via SWIG
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

    # make generated JNI methods available in list of exported functions
    grep -o 'Java.*clangJNI[^\(]*' Index_wrap.cxx >> ../../tools/libclang/libclang.exports
)

# Integrate own Java JNI code
(
    # generate headers for JNI code
    cd src && javac -h ../../llvm-project/clang/include/clang-c -cp .:../generated eu/cqse/clang/ClangBinding.java

    # copy our own native code 
    cp native/eu_cqse_clang_ClangBinding.cpp ../llvm-project/clang/include/clang-c
    
    # make generated JNI methods available in list of exported functions
    cd ../llvm-project/clang/include/clang-c
    grep -o 'Java.*clangJNI[^\(]*' eu_cqse_clang_ClangBinding.h >> ../../tools/libclang/libclang.exports
)

# run cmake
(
    cd ../llvm-project
    mkdir -p build
    cd build
    cmake -DLLVM_ENABLE_PROJECTS=clang -DMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -G "Unix Makefiles" ../llvm
)

# run make
(
    cd ../llbm-project/build
    make -j$CORES
)
