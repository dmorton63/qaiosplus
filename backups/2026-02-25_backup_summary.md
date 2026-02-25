# 2026-02-25 — Backup Summary (local snapshot)

This note summarizes the local backup snapshot created on 2026-02-25.

## Git state captured

- Repo: `/home/dmort/qaiosplusV1`
- Branch: `main` (up to date with `origin/main`)
- HEAD: `95f8e49`
- Working tree: **DIRTY** (includes uncommitted + untracked files)

### Modified / deleted (not staged)
- Deleted: `DO_THIS_FIRST_TODO.md`
- Modified:
  - `QCommand/include/QCCommandRegistry.h`
  - `QCommand/src/QCCommandRegistry.cpp`
  - `QDesktop/include/QDTerminal.h`
  - `QDesktop/src/QDCommandProcessor.cpp`
  - `QDesktop/src/QDTerminal.cpp`
  - `QKMemory/include/QKMemHeap.h`
  - `QKMemory/src/QKMemHeap.cpp`
  - `QKernel/CMakeLists.txt`
  - `build.sh`
  - `kernel/Boot/Desktop/DesktopSession.cpp`
  - `kernel/QKConsole.cpp`
  - `kernel/QKMain.cpp`
  - `kernel/drivers/E1000/QKDrvE1000.cpp`

### Untracked (included)
- `MD_REVIEW_SUMMARY.md`
- `QKernel/include/QKCommandCenter.h`
- `QKernel/src/QKCommandCenter.cpp`
- `jsonFunctionGen.md`
- `jsonFunctionTemplate.md`

## Snapshot artifact

- Archive: `backups/qaiosplus_backup_20260225_094857_95f8e49_dirty.tgz`
- SHA-256: `2058688f6ecf79a3dd504653fd2265cf7bf4747c6631ea929b0589456529f0b6`
- Checksum file: `backups/qaiosplus_backup_20260225_094857_95f8e49_dirty.tgz.sha256`

### What was excluded (to keep size reasonable)
- `./.git`
- `./build` (CMake outputs)
- `./backups` (avoid recursive inclusion)
- `./shared` (runtime host-share artifacts)
- `./tmp`
- `./iso/modules/ramdisk.img` (generated)
- `./tmp_raw_*.bin` (temporary binaries)

## High-level changes since last push (quick human notes)

- Command system: shared `QK::CmdCenter` MVP + enhanced `QC::Cmd::Registry` descriptions.
- Terminal: more robust handling of externally destroyed windows via event listener.
- Heap: safer initialization (guard against double init; early init moved earlier in boot).
- QEMU: enable user-mode networking via e1000 and choose `/usr/bin/qemu-system-x86_64` when present.

## Verify / restore (local)

- Verify checksum:
  - `cd /home/dmort/qaiosplusV1 && sha256sum -c backups/qaiosplus_backup_20260225_094857_95f8e49_dirty.tgz.sha256`
- Restore into a new folder:
  - `mkdir -p /tmp/qaiosplus_restore && tar -xzf backups/qaiosplus_backup_20260225_094857_95f8e49_dirty.tgz -C /tmp/qaiosplus_restore`

