# Codex handoff

## First Codex prompt
Paste this once after opening the repository:

```text
You are implementing MOKO OS v0.1. Read AGENTS.md, README.md, STATUS.md, docs/ARCHITECTURE.md, docs/DESIGN_SYSTEM.md, docs/BUILDING.md and every task file. Do not replace the system with GNOME/KDE or a theme. Start from the first task that is not fully validated. Run tests/builds where the environment permits, fix failures, and update STATUS.md plus the task validation notes. Do not perform destructive disk operations.
```

## Working loop
1. One task at a time.
2. Build/test before marking done.
3. Keep boot and security changes reviewable.
4. Commit logical milestones separately.
5. For hardware-specific bugs, attach `moko-hw-report` output and relevant journal logs.

## Definition of "done"
A file existing is not enough. A task is done only when the relevant build/test has run successfully or the task is explicitly marked `READY (unvalidated)` with the exact missing environment requirement.
