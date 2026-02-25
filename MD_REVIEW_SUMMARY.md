# Markdown Review Summary (2026-02-23)

This file is a *working index* of the repo’s Markdown documents, focused on:
- what looks implemented today (based on code + checked boxes),
- what is still a spec/vision,
- and what to tackle first.

## Quick take
- **The desktop/windowing/style stack is real and fairly far along** (there’s a working `StyleSystem`/`StyleRenderer`, desktop shell, theme overrides, etc.).
- **The “CITADEL” docs are mostly architecture + TODO** (Task_Flow engine, Security Center, and the JSON-secured UI runtime are not implemented as described yet).
- **Command infrastructure exists** (`QC::Cmd::Registry`), but the “Command Center” roadmap in TODOs is only partially realized.
- **Kernel task scheduling is still stub-level** (scheduler selects no next task), so anything that depends on true preemptive multitasking (including parts of Task_Flow) is blocked until scheduler/task switching matures.

---

## Document-by-document status

### README.md
**Type:** project overview + current behavior docs

**Status:** PARTIAL (mostly accurate, but mixed maturity)
- Describes current module layout and build/run flow.
- Theme/desktop JSON loader behavior is described and appears implemented (theme overrides, style snapshot plumbing).

**Best next action:** keep as primary “truth” for how to build/run + current desktop/theme behavior.

### Standards.md
**Type:** conventions

**Status:** DONE (guidance)
- Naming/module conventions that the repo largely follows.

**Best next action:** leave as-is; update only when new module prefixes get added.

### TODO_MAIN.md
**Type:** master engineering TODO list

**Status:** ACTIVE (mixture of done + planned)
- Has several real “done” items already checked.
- Still the best single file for high-level priorities.

**Best next action:** treat as the top-level backlog; link out to other docs rather than duplicating.

### QDesktop/TODO_README.md
**Type:** desktop/UI backlog + renderer roadmap

**Status:** PARTIAL / OUT-OF-DATE in places
- It claims some subsystems are TODO that exist in code now (example: theme loading exists via `QDTheme` using `QCJson`).
- Also contains many accurately marked completed items (graphics abstraction, alpha blending, desktop shell pieces).

**Best next action:** update this doc to reflect current reality (mark what’s done, and move “obsolete” items to a “historical notes” section).

### QWindowing/docs/QWStyleRendererPlan.md
**Type:** implementation plan

**Status:** PARTIALLY IMPLEMENTED
- Core concepts exist in code: `QW::StyleSystem`, `StyleSnapshot`, and `StyleRenderer` integration in windows/desktop.

**Best next action:** adjust the plan so “Phase 0/1” reflects what’s already landed, then list the remaining renderer/style features as TODOs.

### CITADEL_UI_RUNTIME.md
**Type:** architecture spec + ordered TODO

**Status:** VISION (with partial overlap)
- The *exact* “universal JSON window loader + signed/protected UI assets + permissions enforcement” is not implemented yet.
- Some building blocks *do* exist elsewhere (JSON parser in `QCJson`, style snapshot distribution, desktop JSON theme overrides), but not the full security/policy pipeline.

**Best next action:** keep as the spec, but add a short “Current Reality” section at top so it doesn’t read like it already exists.

### CITADEL_UI_TODO.md
**Type:** condensed ordered checklist (same content as the TODO section in CITADEL_UI_RUNTIME)

**Status:** VISION

**Best next action:** either delete/merge into `CITADEL_UI_RUNTIME.md`, or keep it but make it a thin index that links back to the full spec.

### CITADEL_TASKFLOW.md
**Type:** Task_Flow execution engine concept

**Status:** VISION
- No `Task_Flow`/`Task_Node` implementation in codebase.

**Best next action:** keep as concept; do not start implementation until kernel scheduling/task execution primitives are further along.

### CITADEL_TASKFLOW_TODO.md
**Type:** Task_Flow checklist

**Status:** NOT STARTED

**Best next action:** park this until scheduling/task execution and a stable async execution model exists.

### LOG_DOC.md
**Type:** security architecture brainstorm

**Status:** VISION (with some underlying TPM plumbing in code)
- The “Security Center” (isolated, message-driven subsystem) is not implemented.
- TPM code exists in boot area and can be reused later for “sealing” style workflows.

**Best next action:** keep as internal design guidance; later, split into (1) stable spec and (2) brainstorming notes.

### TODO_S_LIST.md
**Type:** security implementation tasks (very detailed)

**Status:** NOT STARTED (as a unified subsystem)
- Some prerequisites exist (TPM code, event system), but the SC subsystem and provisioning/token model are not wired end-to-end.

**Best next action:** treat as a “vNext” backlog; start with a minimal SC skeleton only when you’re ready to commit to the architecture.

### backups/2026-02-19_backup_summary.md
**Type:** historical snapshot

**Status:** DONE (reference)

**Best next action:** keep as historical context.

### code_snippets/2026-02-16_security-ui_backup.md and Shared/2026-02-16_security-ui_backup.md
**Type:** snippet backups

**Status:** DONE (reference)

**Best next action:** optionally consolidate into one location to avoid duplication.

---

## Missing doc noted
- `DO_THIS_FIRST_TODO.md` is not present in the repo root (it appeared in earlier structure notes, but it’s not currently in the filesystem).

---

## What to tackle first (recommended order)

1) **Command Center MVP (small, high leverage)**
- Why: improves usability immediately; enables `startx`/`stopx`, mode toggles, diagnostics, and future GUI integration.
- Good news: the command registry/dispatch already exists (`QC::Cmd::Registry`).
- Deliverable: ship a starter command set (`help`, `clear`, `showmode`, `startx`, `stopx`, `cat`, `ls`, etc.) and wire it consistently across terminal surfaces.

2) **Image Reader service (unblocks branding + icons + wallpaper)**
- Why: blocks “CITADEL” visual identity work and desktop polish.
- Scope can be staged: start with `.bmp` (easy) then `.png`.

3) **Filesystem mounting + external block devices (enables real persistence)**
- Why: makes `/shared` and other media usable inside the OS, and makes command tooling more meaningful.

4) **Security Center skeleton (only after you want to commit)**
- Why: big architectural commitment; should be started only when you’re ready to wire it into boot + event system.

5) **Task_Flow engine (after scheduler/tasking is real)**
- Why: depends on robust scheduling primitives and safe concurrency.

