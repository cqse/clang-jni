# clang-jni
JNI bindings for clang

## Build steps

On a linux machine do the following

- `git clone https://github.com/cqse/clang-jni.git`
- `cd clang-jni`
- `./prepare-linux.sh`

This will install all required packages, clone the LLVM repository,
run SWIG to generate the JNI wrapper, and create a package
clang-build-package.tar.gz at the base directory.

Copy this package to a windows machine and a mac, follow the
instructions in `build-linux.sh`, `build-windows.md` and
`build-mac.md`.

Copy all the resulting libraries back to the linux machine and execute
`build-java.sh`.