# Documentation Path Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align Markdown documentation paths and directory descriptions with the current repository layout.

**Architecture:** Keep this as a documentation-only cleanup. Current entry-point documentation should describe the actual tree, while historical handoff documents should keep their historical context but use valid current paths or an explicit note.

**Tech Stack:** Markdown, repository-relative paths.

## Global Constraints

- Do not modify Python source, tests, configs, images, or PDFs.
- Use current repository root paths such as `2023e/src/vision/...` for cross-project references.
- Use project-local paths such as `src/vision/...` only where the text explicitly assumes commands run from `2023e/`.
- Preserve historical handoff context; do not rewrite old status narratives beyond path corrections and short historical notes.

---

### Task 1: Refresh Active README Files

**Files:**
- Modify: `2023e/README.md`
- Modify: `2023e/scripts/README.md`

**Interfaces:**
- Consumes: Current repository tree under `2023e/`.
- Produces: Accurate entry-point documentation for future readers.

- [ ] Replace stale `2023e/` tree header with `2023e/`.
- [ ] Update `src/`, `tests/`, and `scripts/` descriptions to reflect existing Vision Layer code, tests, and preview/debug scripts.
- [ ] Update current-stage wording so it no longer says the project is only a skeleton.

### Task 2: Correct Broken Repository Paths

**Files:**
- Modify: Markdown files under `2023e/docs/`

**Interfaces:**
- Consumes: Existing Markdown path references.
- Produces: Valid repository-relative references.

- [ ] Replace legacy `projects/2023e/` references with `2023e/` in Markdown docs.
- [ ] Replace legacy `plan/2023e/plan_v0.md` references with `2023e/docs/plan/2023e/plan_v0.md`.
- [ ] Replace legacy `plan/2023e/handoff.md` references with `2023e/docs/plan/2023e/handoff.md`.
- [ ] Replace legacy `todos/E_运动目标控制与自动追踪系统.pdf` references with `2023e/docs/E_运动目标控制与自动追踪系统.pdf`.

### Task 3: Mark Historical Handoff Context

**Files:**
- Modify: `2023e/docs/hands-off/2026-07-05-architecture-handoff.md`
- Modify: `2023e/docs/hands-off/2026-07-05-vision-implementation-handoff.md`

**Interfaces:**
- Consumes: Historical documents that mention an early skeleton state.
- Produces: Documents that remain historically accurate without misleading readers about current state.

- [ ] Add a short note near the top explaining that the document records the state at handoff time and current paths live under `2023e/`.

### Task 4: Verify Cleanup

**Files:**
- Read-only verification over Markdown files.

**Interfaces:**
- Consumes: Updated docs.
- Produces: Evidence that stale path patterns are gone or intentionally historical.

- [ ] Search for `2023e`.
- [ ] Search for `plan/2023e/`.
- [ ] Search for `2023e/docs/E_运动目标控制与自动追踪系统.pdf`.
- [ ] Search for active-doc stale phrases such as `当前只保留目录说明`, `当前阶段不写实现`, and `reserved for future`.
