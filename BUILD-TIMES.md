# Build Times and System Specs

Reference timings for planning build machine usage.
After each build, please update the row for your platform.

| Platform | System | LLVM version | Build time (cmake + compile + link) | Date |
|----------|--------|-------------|-------------------------------------|------|
| Windows  | Intel Xeon @ 2.20 GHz, 2 cores / 4 threads, 16 GB RAM, Windows Server 2022 Datacenter | 20.1.8 | ~2h 45m (full build with `--parallel`) | 2026-03-19 |
| Linux    | *(please fill in after building)* | | | |
| macOS    | *(please fill in after building)* | | | |

Expect the first full build to take 2-3 hours on a 2-core machine.
Incremental rebuilds (e.g., after patching a single file) take only a few minutes.
