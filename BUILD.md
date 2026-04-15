# Build Workflow for clang-jni

This documents the full cross-platform build workflow for the clang JNI
bindings. The build produces native libraries for Linux, Windows, and macOS,
which are packaged into a single JAR for use in Teamscale.

The workflow uses **git commits on a shared branch** to transfer native
libraries between build machines. Each platform builds its native library,
commits it to `libs/`, and pushes. The final JAR is assembled on one machine,
and then the intermediate library commits are squashed away.

### Why squash?

The native libraries (`.so`, `.dylib`, `.dll`) are large (~50-90 MB each) and
are tracked by Git LFS. The final JAR already contains all three libraries
(ZIP-compressed). Keeping both the individual libraries and the JAR in git
history would double the LFS storage for every build (~350 MB per version).

By squashing the intermediate commits into a single JAR-only commit, we use
git as a convenient transfer mechanism between machines, but the final history
only contains the JAR (~76 MB per version). This keeps LFS usage manageable.

## Overview

```
Step 1: Linux   — prepare (SWIG, patches) + build + test + commit & push
Step 2: Windows — pull + build + commit & push
Step 3: macOS   — pull + build + commit & push
Step 4: Any machine — pull all binaries + build JAR + remove libs + squash + force push
```

## Prerequisites

| Platform | Required software |
|----------|-------------------|
| Linux    | g++, cmake, openjdk-8-jdk-headless, swig 4.3.1, python3 |
| Windows  | Visual Studio 2022 (C++ workload), CMake 3.20+, JDK 11+, Python 3, Git |
| macOS    | Xcode command line tools, CMake, JDK 11+, gsed (`brew install gnu-sed`) |

## Step 1: Linux — Prepare, Build, and Test

This is always the first step. It clones LLVM, runs SWIG to generate JNI
bindings, applies patches, and builds the Linux binary.

```bash
# Clone the repo (or pull latest on existing checkout)
git clone https://github.com/cqse/clang-jni.git
cd clang-jni
git checkout ts/44325_clang_update   # or whatever the working branch is

# IMPORTANT: If ../llvm-project already exists from a previous build attempt,
# reset it to the pristine checkout before running prepare, to avoid applying
# patches (memory leak fix, stderr suppression, CMakeLists edits) twice:
#   cd ../llvm-project && git reset --hard && cd ../clang-jni

# Run the prepare script (clones llvm-project as sibling, runs SWIG, applies patches)
# This must run from the clang-jni directory. llvm-project will be cloned at ../llvm-project
./prepare-linux.sh

# Build the native library
./build-linux.sh

# Test (run the integration test against the built library)
# The loader extracts the .so from the classpath, so we compile first and
# place the native lib alongside the .class files.
mkdir -p build
javac -d build -source 1.8 -target 1.8 -cp src:generated:test $(find src generated test -name '*.java')
cp libs/libclang-linux.so build/eu/cqse/clang/
java -cp build --enable-native-access=ALL-UNNAMED eu.cqse.clang.ClangJniTest

# Commit and push
git add libs/libclang-linux.so generated/ src/
git commit -m "Build Linux binary for clang <version>"
git push origin ts/44325_clang_update
```

### What prepare-linux.sh does (for reference)
1. Installs build dependencies via apt
2. Clones llvm-project at the specified tag (e.g., `llvmorg-20.1.8`) into `../llvm-project`
3. Runs SWIG to generate Java + C++ JNI bindings from `native/clang.i`
4. Copies custom JNI code (`native/eu_cqse_clang_ClangBinding.cpp`) into the llvm tree
5. Generates JNI headers from `src/eu/cqse/clang/ClangBinding.java`
6. Applies patches (memory leak fix, stderr suppression)
7. Registers JNI exports and integrates into CMakeLists.txt

### What build-linux.sh does
1. Patches CMakeLists.txt with JDK include paths
2. Runs cmake + make
3. Copies `libclang.so` to `libs/libclang-linux.so`

## Step 2: Windows — Build

The Windows build reuses the SWIG-generated code committed by the Linux step.
It does NOT need SWIG installed, but it does need to clone llvm-project and
integrate the JNI code + patches itself.

### 2.1 Setup

```
# Clone the repo
git clone https://github.com/cqse/clang-jni.git
cd clang-jni
git checkout ts/44325_clang_update
git pull    # get the Linux binary and generated code

# Clone llvm-project as sibling (or reset if it already exists from a previous attempt)
cd ..
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
git reset --hard    # if re-using an existing clone, run this before the integration steps
git checkout llvmorg-20.1.8    # must match the tag used in prepare-linux.sh
cd ..\clang-jni
```

### 2.2 Integrate JNI code into llvm-project

These steps replicate what `prepare-linux.sh` does, but without running SWIG.
Run from the `clang-jni` directory (with `llvm-project` as sibling).
Use Git Bash for the shell commands.

The SWIG-generated C++ file (`clang-jni.cpp`) and Java files are both
committed in the `generated/` directory. No SWIG installation is needed on
Windows or macOS.

```bash
# Copy SWIG-generated C++ into the llvm tree
cp generated/clang-jni.cpp ../llvm-project/clang/tools/libclang/

# Register SWIG exports
grep -o 'Java.*ClangJNI[^\(]*' \
  ../llvm-project/clang/tools/libclang/clang-jni.cpp \
  >> ../llvm-project/clang/tools/libclang/libclang.exports

# Generate JNI headers for ClangBinding
(cd src && javac -h ../../llvm-project/clang/tools/libclang \
  -cp .:../generated eu/cqse/clang/ClangBinding.java)

# Copy native code into llvm tree
cp native/eu_cqse_clang_ClangBinding.cpp ../llvm-project/clang/tools/libclang/
mv ../llvm-project/clang/include/clang-c/clang-jni.cpp \
   ../llvm-project/clang/tools/libclang/

# Register ClangBinding exports
grep -o 'Java_eu_cqse_clang_ClangBinding[^\(]*' \
  ../llvm-project/clang/tools/libclang/eu_cqse_clang_ClangBinding.h \
  >> ../llvm-project/clang/tools/libclang/libclang.exports

# Patch CMakeLists.txt to include our source files
cd ../llvm-project/clang/tools/libclang
sed -i '/Indexing.cpp/a eu_cqse_clang_ClangBinding.cpp' CMakeLists.txt
sed -i '/Indexing.cpp/a clang-jni.cpp' CMakeLists.txt
sed -i '/Index_Internal.h/a eu_cqse_clang_ClangBinding.h' CMakeLists.txt
sed -i '/set.LIBS/i include_directories(../../include/clang-c)' CMakeLists.txt
cd ../../../../clang-jni
```

### 2.3 Apply patches

Apply from the `llvm-project` directory:

**Patch 1: Memory leak fix** (`clang/tools/libclang/clang-jni.cpp`)
Find the `delete_1CXUnsavedFile` function and add before `delete arg1;`:
```cpp
if (arg1 != 0) {
    if (arg1->Contents != 0) delete arg1->Contents;
    if (arg1->Filename != 0) delete arg1->Filename;
}
```

**Patch 2: Suppress stderr** (`llvm/lib/Support/raw_ostream.cpp`)
In the `errs()` function, replace the `static raw_fd_ostream S(STDERR_FILENO, false, true);`
line with:
```cpp
std::error_code EC;
#ifdef _WIN32
static raw_fd_ostream S("NUL", EC);
#else
static raw_fd_ostream S("/dev/null", EC);
#endif
```

### 2.4 Windows-specific CMake adjustments

Edit `llvm-project/clang/tools/libclang/CMakeLists.txt`:

```cmake
# Add JDK include paths (adjust to your actual JDK install location)
include_directories("C:/Program Files/Java/jdk-XX/include")
include_directories("C:/Program Files/Java/jdk-XX/include/win32")
```

The `-fexceptions` flag (if added by prepare-linux.sh) is GCC-specific.
On Windows, either remove it or wrap it:
```cmake
if(NOT MSVC)
    target_compile_options(libclang PUBLIC "-fexceptions")
endif()
```

### 2.5 Build

**IMPORTANT:** Use `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` to link the
C runtime statically (`/MT`). Without this, the DLL depends on VCRUNTIME140.dll,
and Java will fail to load it with "DLL initialization routine failed" because
the JDK ships an older VCRUNTIME140.dll (v14.29) that conflicts with the one
from MSVC 2022 (v14.40).

```
cd llvm-project
mkdir build
cd build
cmake -DLLVM_ENABLE_PROJECTS="clang" ^
  -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=X86 ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
  -G "Visual Studio 17 2022" -A x64 -Thost=x64 ..\llvm

Measure-Command { cmake --build . --config Release --target libclang --parallel }
```

The `Measure-Command` wrapper prints the elapsed time at the end.
Please record the build time in `BUILD-TIMES.md`.

Or open `build\LLVM.sln` in Visual Studio, set config to Release, and build
the `libclang` target.

**Tip:** The build takes 2-3 hours. Run the build command in a **separate
PowerShell or cmd window** rather than from within Claude Code. Long-running
background builds inside Claude Code's shell can cause the session to become
unresponsive as it waits for output.

PowerShell command (run from any directory):
```powershell
& "C:\Program Files\CMake\bin\cmake.exe" --build C:\Users\user\Desktop\clang-jni-work\llvm-project\build --config Release --target libclang --parallel
```

### 2.6 Copy binary, test, and commit

```
# The DLL may be in one of these locations — check both:
#   build\Release\bin\libclang.dll
#   build\lib\Release\libclang.dll
copy <path>\libclang.dll ..\clang-jni\libs\libclang-win.dll

cd ..\clang-jni

# Test the binary (no JAR build needed)
mkdir build
javac -d build -source 1.8 -target 1.8 -cp src;generated;test (get-childitem src,generated,test -recurse -filter *.java | %{$_.FullName})
copy libs\libclang-win.dll build\eu\cqse\clang\
java -cp build --enable-native-access=ALL-UNNAMED eu.cqse.clang.ClangJniTest

git add libs/libclang-win.dll
git commit -m "Build Windows binary for clang <version>"
git push origin ts/44325_clang_update
```

## Step 3: macOS — Build

Same approach as Windows: reuses SWIG-generated code from the Linux step.

### 3.1 Setup

```bash
git clone https://github.com/cqse/clang-jni.git
cd clang-jni
git checkout ts/44325_clang_update
git pull

cd ..
git clone https://github.com/llvm/llvm-project.git
cd llvm-project
git reset --hard    # if re-using an existing clone, run this before the integration steps
git checkout llvmorg-20.1.8
cd ../clang-jni
```

### 3.2 Integrate JNI code + Apply patches

Same steps as Windows section 2.2 and 2.3 (the SWIG-generated `clang-jni.cpp`
is in `generated/` — no SWIG needed). On macOS, use `gsed` instead of `sed`
(install via `brew install gnu-sed`), or use `sed -i '' -e '...'`.

### 3.3 macOS-specific CMake adjustments

Edit `llvm-project/clang/tools/libclang/CMakeLists.txt`:

```cmake
# Add JDK include paths (use /usr/libexec/java_home to find the path)
include_directories(/path/to/jdk/include)
include_directories(/path/to/jdk/include/darwin)
```

### 3.4 Fix macOS export list

On macOS, the linker uses an export list generated from `libclang.map` via
`linker-script-to-export-list.py`. Both only match `clang_*` symbols by
default, so our `Java_*` JNI symbols would be silently dropped. Fix both:

```bash
cd ../llvm-project/clang/tools/libclang

# Add JNI exports to libclang.map (insert before the closing };)
python3 -c "
with open('libclang.exports') as f:
    jni = [l.strip() for l in f if l.strip().startswith('Java_')]
with open('libclang.map') as f:
    c = f.read()
c = c.replace('};', '\n'.join(f'    {s};' for s in jni) + '\n};', 1)
with open('libclang.map', 'w') as f:
    f.write(c)
"

# Patch the export list generator to also match Java_ symbols
gsed -i 's/(clang_\[/((?:clang_|Java_)[/' linker-script-to-export-list.py

cd ../../../../clang-jni
```

### 3.5 Build

```bash
cd ../llvm-project
mkdir -p build
cd build
cmake -DLLVM_ENABLE_PROJECTS="clang" \
  -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD=AArch64 \
  -G "Unix Makefiles" ../llvm
make -j$(sysctl -n hw.ncpu) libclang
cd ../../clang-jni
cp ../llvm-project/build/lib/libclang.dylib libs/libclang-mac.dylib
```

### 3.6 Test, commit, and push

```bash
# Test the binary (no JAR build needed)
mkdir -p build
javac -d build -source 1.8 -target 1.8 -cp src:generated:test $(find src generated test -name '*.java')
cp libs/libclang-mac.dylib build/eu/cqse/clang/
java -cp build --enable-native-access=ALL-UNNAMED eu.cqse.clang.ClangJniTest

git add libs/libclang-mac.dylib
git commit -m "Build macOS binary for clang <version>"
git push origin ts/44325_clang_update
```

## Step 4: Build the Final JAR, Remove Libraries, Squash, and Force Push

This step can run on any machine that has all three binaries.
It builds the JAR, removes the native libraries from git, squashes the
intermediate build commits into a single clean commit, and force pushes.

**This is a destructive operation (force push). Make sure no one else is
pushing to the branch at the same time.**

### 4.1 Pull all binaries and build the JAR

```bash
cd clang-jni
git pull    # get Windows and macOS binaries

# Verify all binaries are present
ls -la libs/libclang-linux.so libs/libclang-win.dll libs/libclang-mac.dylib

# Build the JAR (compiles Java + bundles native libs)
./build-java.sh

# Verify the JAR contains all three native libraries
jar tf clang-jni.jar | grep libclang
# Should show:
#   eu/cqse/clang/libclang-linux.so
#   eu/cqse/clang/libclang-mac.dylib
#   eu/cqse/clang/libclang-win.dll
```

### 4.2 Remove the native libraries from git

The JAR now contains all three libraries. The individual files in `libs/`
are no longer needed in git history. Remove them:

```bash
git rm libs/libclang-linux.so libs/libclang-mac.dylib libs/libclang-win.dll
git add clang-jni.jar
git commit -m "Build final JAR with all platform binaries, remove intermediate libs"
```

### 4.3 Squash the build commits

The previous steps created several intermediate commits (one per platform
library, plus the final JAR commit). These intermediate commits contain
large LFS objects that we don't want in the final history.

Find the commit hash **before** the first build commit on this branch
(i.e., the last "real" commit before the platform builds started):

```bash
# Show recent commits to identify the squash range
git log --oneline -10

# Interactive rebase to squash all build commits into one.
# Replace <base-commit> with the hash of the commit BEFORE the first
# platform build commit (e.g., the "Upgrade to LLVM X" commit).
git rebase -i <base-commit>
```

In the interactive rebase editor:
- Keep the first build commit as `pick`
- Change all subsequent build commits to `squash` (or `s`)
- Save and close
- Write a combined commit message like: "Build clang-jni JAR for LLVM 21.1.8 (Linux + macOS + Windows)"

### 4.4 Force push

The rebase rewrote history, so a force push is required:

```bash
git push --force-with-lease origin <branch-name>
```

`--force-with-lease` is safer than `--force` — it fails if someone else
pushed to the branch since your last pull.

### 4.5 Verify

After force pushing, verify the branch looks clean:

```bash
# Should show a single commit for the build (not one per platform)
git log --oneline -5

# The JAR should be present, individual libs should NOT be present
ls clang-jni.jar        # should exist
ls libs/libclang-*.so   # should NOT exist in working tree
```

### Why this matters

Without squashing, every build leaves ~350 MB of LFS objects in history
(3 native libraries + 1 JAR). With squashing, only the JAR (~76 MB)
remains. Over multiple LLVM version updates, this prevents the LFS storage
from growing out of control.

## Step 5: Publish to the Maven Repository

Upload the JAR **and a matching `.pom` file** to the Maven repository
under `eu/cqse/clang-jni/<version>/`. Use [`pom-template.xml`](pom-template.xml)
and replace `VERSION` with the target Maven version.

The POM is required because the Teamscale build's `collectBackendLicenses`
task reads the `<licenses>` block from it. Without the POM (or with a POM
missing that block), the Teamscale build fails with
`License not found for eu.cqse:clang-jni: null`.

Notes:
- Never overwrite an existing version — bump to `-1`, `-2`, … instead.
  Nexus caches aggressively, and overwrites can cause stale reads.
- The Maven `<version>` is independent of `ClangJniLoader.VERSION` (which
  encodes the LLVM/SWIG versions). You can republish the same JAR under
  a new Maven version if only POM metadata needs fixing.
- If Teamscale CI still fails after a correct upload, either:
  - ask `#dev-infra` to invalidate the Nexus cache entry for
    `eu/cqse/clang-jni/<version>/`, or
  - start a manual build pipeline with `--rerun-tasks` in the
    `GRADLE_EXTRA_ARGS` pipeline variable to bypass the Gradle build
    cache (which may hold a stale SBOM generated before the POM was
    uploaded).

## Troubleshooting

### Generated code
The SWIG-generated files (Java in `generated/eu/cqse/clang/`, C++ in
`generated/clang-jni.cpp`) are committed to the repo. They only need to be
regenerated if `native/clang.i` changes. Use SWIG 4.3.1 on Linux for that.

### Windows: -fexceptions compiler error
This is a GCC flag. Wrap it in `if(NOT MSVC)` — see section 2.4.

### Windows: unresolved symbol errors
Check that all JNI exports are listed in `libclang.exports`. Compare with
`dumpbin /exports libclang.dll`.

### macOS: sed -i fails
macOS BSD sed requires `sed -i '' -e '...'`. Use `gsed` from Homebrew instead.

### Windows: "DLL initialization routine failed" in Java
The JDK bundles its own (older) copy of VCRUNTIME140.dll. If libclang is
built with the default dynamic CRT (`/MD`), the version mismatch causes
`DllMain` to fail when loaded via `System.load()`. Fix: build with
`-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` (static CRT, `/MT`).
The DLL loads fine from Python/C++ regardless — this is Java-specific.

### SWIG version
Use SWIG 4.3.1 for consistency. The generated Java files in `generated/`
should match across platforms.

## Build Times

See [BUILD-TIMES.md](BUILD-TIMES.md) for reference timings.
**Please update that file after each build.**
