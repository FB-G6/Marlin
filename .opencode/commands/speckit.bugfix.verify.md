---
description: Verify that bugfix patches are consistent across all spec artifacts
---


<!-- Extension: bugfix -->
<!-- Config: .specify/extensions/bugfix/ -->
# Verify Bugfix Consistency

Verify that all spec artifacts are consistent after bugfix patches — checks that new requirements have corresponding plan sections and tasks, reopened tasks are not still marked complete, and no orphaned references exist.

## User Input

```text
$ARGUMENTS
```

ALL outputs MUST be in Russian.

You **MUST** consider the user input before proceeding (if not empty). The user may specify a bug ID to verify (e.g., "BUG-001") or "all" to check everything.

## Prerequisites

1. Verify a spec-kit project exists by checking for `.specify/` directory
2. Locate the current feature's spec directory
3. Verify at least `spec.md` exists

## Outline

1. **Load all artifacts**: Read from the current feature directory:
   - `spec.md` — the source of truth
   - `plan.md` — implementation plan (if exists)
   - `tasks.md` — task breakdown (if exists)
   - `specs/{feature}/bugs/*.md` — all bug reports

2. **Check bug report status**: For each bug report file:

   | Check | Pass Condition | Fail Condition |
   |-------|---------------|----------------|
   | Report exists | Bug file is in `bugs/` directory | Missing bug file |
   | Report is patched | Has `**Status**: Patched` marker | Still `Open` or missing status |
   | Patch date present | Has `**Patched**: [DATE]` entry | Missing patch date |

3. **Verify spec.md consistency**: For each bugfix note in spec.md:
   - New requirements have clear acceptance criteria
   - Strikethrough items have a reason documented
   - No duplicate requirements introduced by the patch
   - Bug IDs in notes match existing bug report files

4. **Verify plan.md consistency** (if exists):
   - Every new requirement in spec.md has a corresponding plan section or note
   - No plan sections reference removed or superseded requirements
   - Bugfix notes in plan.md match those in spec.md

5. **Verify tasks.md consistency** (if exists):
   - Every new requirement in spec.md is traceable to at least one task
   - No tasks marked `[x]` that were supposed to be reopened
   - Reopened tasks have the `(reopened — BUG-NNN)` annotation
   - New task IDs are sequential and do not duplicate existing IDs
   - Task dependencies form a valid DAG (no circular dependencies)
   - Wave DAG (if present) includes all new tasks

6. **Output verification report**:

   ```markdown
   # Bugfix Verification Report

   **Feature**: [Feature name]
   **Date**: [DATE]

   ## Bug Reports
   | Bug ID | Title | Status | Patched |
   |--------|-------|--------|---------|
   | BUG-001 | [Title] | ✅ Patched | [DATE] |
   | BUG-002 | [Title] | ⚠️ Open | — |

   ## Consistency Checks

   | Check | Result | Details |
   |-------|--------|---------|
   | All bugs patched | ✅ Pass / ⚠️ Fail | [N patched, M open] |
   | Spec requirements covered | ✅ Pass / ⚠️ Fail | [N requirements, all have tasks] |
   | No false completions | ✅ Pass / ⚠️ Fail | [N tasks verified] |
   | Reopened tasks annotated | ✅ Pass / ⚠️ Fail | [N reopened, all annotated] |
   | Task IDs sequential | ✅ Pass / ⚠️ Fail | [Last ID: TNNN] |
   | No circular dependencies | ✅ Pass / ⚠️ Fail | [DAG valid] |
   | Cross-artifact traceability | ✅ Pass / ⚠️ Fail | [All specs → plan → tasks] |

   ## Recommended Actions
   - [List any issues found and how to fix them]
   - If all checks pass: Resume `/speckit.implement` to apply the code fix
   ```

7. **Report**: Output the verification report. Do not modify any files — this command is read-only.

## Rules

- **Read-only** — this command never modifies any files
- **Check all artifacts** — verify consistency across spec, plan, tasks, and bug reports
- **Be specific about failures** — report exact bug IDs, task IDs, and requirement text that fail checks
- **Handle missing artifacts gracefully** — if plan.md or tasks.md does not exist, skip those checks and note the absence
- **Verify traceability** — every bugfix note must reference a valid bug report, and every bug report must have corresponding artifact changes