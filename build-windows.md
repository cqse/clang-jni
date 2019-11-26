# Build instructions for windows

Last tested with Windows Server 2019 Datacenter

Starting point is the package created via prepare-linux.sh on a
linux machine. We use this prepared package, to keep the number
of required tools on windows down to a minimum.

More info: https://clang.llvm.org/get_started.html

# Required software

- Visual Studio 2019 (https://visualstudio.microsoft.com/)
- Oracle Java
- Python (https://www.python.org/downloads/)
- cmake (https://cmake.org/download/)

# Compilation

- Create a new directory (e.g. c:\clang) and switch to it
- Extract the mentioned package to this directory
- Adjust the file `llvm-project/clang/tools/libclang/CMakeLists.txt` by inserting the following two lines (or similar) before the line starting with `set(LIBS`:
    - `include_directories("c:/Program Files/Java/jdk-12.0.1/include")`
    - `include_directories("c:/Program Files/Java/jdk-12.0.1/include/win32")`
- `cd llvm-project`
- `mkdir build`
- `cd build`
- `cmake -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra" -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 -G "Visual Studio 16 2019" -A x64 -Thost=x64 ../llvm`

Open the `LLVM.sln` in the build directory with Visual Studio. Set the
build type to *Release* and build the *libclang* target.
