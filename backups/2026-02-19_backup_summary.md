# 2026-02-19 — Backup Summary (GitHub)

This note summarizes the work backed up to GitHub on 2026-02-19.

## Highlights

### Build / CMake restructure
- Migrated QC “namespace” libraries into distinct folders:
  - `QCCore/` (types, string/mem, logger, builtins)
  - `QCMath/` (headers currently; `src/` empty)
  - `QCSerialization/` (JSON + overlay)
  - `QUICommon/` (UI style/common helpers)
  - `QCommand/` (command registry)
- Kept compatibility by defining `QCommon` as an INTERFACE umbrella target that links the above libraries.

### Permanent fix for CPU relax / spin delays
- Added a low-level `cpu_relax()` implementation in NASM using `pause; ret`.
- Exposed `extern "C" void cpu_relax();` for C++ callers.
- Replaced deprecated `volatile` delay loops (and removed reliance on `__builtin_cpu_relax()`) in:
  - UHCI driver
  - XHCI driver

### Startup/QEMU/dev workflow improvements (in working tree)
- Added `startup.cfg` handling (ramdisk + kernel parsing) to support selecting boot/startup mode.
- Updated `build.sh` to optionally attach a host `shared/` folder to QEMU.

### Desktop/UI related work (in working tree)
- Theme overrides expanded to support transparency + font sizing.
- Painter text scaling support added and wired through style snapshot.
- Desktop terminal gained a minimal `ls` command.

## Notes
- The local QEMU host-share directory `shared/` is ignored by git to prevent runtime artifacts (logs/transcripts) from being accidentally pushed.

## Restore
- `git clone` the repo
- `cmake -B build -S .`
- `cmake --build build`
- `./build.sh -r`
