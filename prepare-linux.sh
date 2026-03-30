#!/bin/bash

# This is the main linux script for preparing the build of the JNI
# native part for the clang bindings. This is intended to be run on a
# fresh Ubunutu installation (last tested with 18.04), such as a cloud
# machine. Please read the lines/instructions carefully, before
# executing the script. The script should be executed from the main
# directory of the git and will clone LLVM besides it.

# The tag used to compile from
LLVM_TAG=llvmorg-21.1.8 

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

    cp native/clang.i ../llvm-project/clang/include/clang-c
    cd ../llvm-project/clang/include/clang-c

    # run swig to generate the JNI binding
    /usr/local/bin/swig -c++ -java -package eu.cqse.clang -outdir ../../../../clang-jni/$GENERATED_DIR -o clang-jni.cpp -v -Wall clang.i

    # make generated JNI methods available in list of exported functions
    grep -o 'Java.*ClangJNI[^\(]*' clang-jni.cpp >> ../../tools/libclang/libclang.exports
)

echo "Integrating own Java JNI code"
(
    # generate headers for JNI code
    (cd src && javac -h ../../llvm-project/clang/tools/libclang -cp .:../generated eu/cqse/clang/ClangBinding.java)

    # copy our own native code 
    cp native/eu_cqse_clang_ClangBinding.cpp ../llvm-project/clang/tools/libclang
    mv ../llvm-project/clang/include/clang-c/clang-jni.cpp ../llvm-project/clang/tools/libclang
    # Also keep a copy in generated/ for Windows/macOS builds (no SWIG needed)
    cp ../llvm-project/clang/tools/libclang/clang-jni.cpp generated/

    # patch generated JNI to fix resource leak
    cat <<EOF | (cd ../llvm-project && patch -p1)
diff --git a/clang/tools/libclang/clang-jni.cpp b/clang/tools/libclang/clang-jni.cpp
index 279f9e62b56..1d668f0c201 100644
--- a/clang/tools/libclang/clang-jni.cpp
+++ b/clang/tools/libclang/clang-jni.cpp
@@ -1998,7 +1998,11 @@ SWIGEXPORT void JNICALL Java_eu_cqse_clang_ClangJNI_delete_1CXUnsavedFile(JNIEnv
   
   (void)jenv;
   (void)jcls;
-  arg1 = *(CXUnsavedFile **)&jarg1; 
+  arg1 = *(CXUnsavedFile **)&jarg1;
+  if (arg1 != 0) {
+    if (arg1->Contents != 0) delete arg1->Contents;
+    if (arg1->Filename != 0) delete arg1->Filename;
+  }
   delete arg1;
 }
EOF
    
    # make generated JNI methods available in list of exported functions
    cd ../llvm-project/clang/tools/libclang
    grep -o 'Java_eu_cqse_clang_ClangBinding[^\(]*' eu_cqse_clang_ClangBinding.h >> libclang.exports

    # Monkey patching our build steps into existing cmake files
    sed -i -e '/Indexing.cpp/a eu_cqse_clang_ClangBinding.cpp' CMakeLists.txt
    sed -i -e '/Indexing.cpp/a clang-jni.cpp' CMakeLists.txt
    sed -i -e '/Index_Internal.h/a eu_cqse_clang_ClangBinding.h' CMakeLists.txt
    sed -i -e '/set.LIBS/i include_directories(../../include/clang-c)' CMakeLists.txt
    # Enable C++ exception handling for our JNI source files only.
    # LLVM is built with -fno-exceptions globally, but our JNI code uses
    # try/catch as a safety net to translate C++ errors into Java exceptions.
    # Using set_source_files_properties applies -fexceptions only to our files,
    # avoiding contradictory flags on the same compilation unit.
    sed -i -e '/if.ENABLE_SHARED/i set_source_files_properties(clang-jni.cpp eu_cqse_clang_ClangBinding.cpp PROPERTIES COMPILE_OPTIONS "-fexceptions")' CMakeLists.txt

    # Add JNI symbols to the linker version script (libclang.map).
    # On Linux the linker uses this to control symbol visibility.
    # Without this, Java_* symbols are not exported from libclang.so.
    echo "" >> libclang.map
    echo "CQSE_JNI {" >> libclang.map
    echo "  global:" >> libclang.map
    grep -o 'Java[^(]*' libclang.exports | sed 's/^/    /; s/$/;/' >> libclang.map
    echo "};" >> libclang.map
)

echo "Patching raw_ostream to suppress unwanted output"
(
    cd ../llvm-project/llvm/lib/Support
    # In clang 20, errs() returns raw_fd_ostream& so we can't return nulls().
    # Redirect to /dev/null (or NUL on Windows) using a platform-aware ifdef.
    # In LLVM 21+, errs() already has a local 'EC' variable (for z/OS auto-conversion),
    # so we use 'EC_devnull' to avoid a redeclaration error.
    sed -i 's|static raw_fd_ostream S(STDERR_FILENO, false, true);|std::error_code EC_devnull;\n#ifdef _WIN32\n  static raw_fd_ostream S("NUL", EC_devnull);\n#else\n  static raw_fd_ostream S("/dev/null", EC_devnull);\n#endif|' raw_ostream.cpp
)

echo "Preparation complete. Run build-linux.sh next."

