---
description: Surgically update spec, plan, and tasks to address the reported bug
---


<!-- Extension: bugfix -->
<!-- Config: .specify/extensions/bugfix/ -->
# Patch Spec Artifacts

Surgically update spec.md, plan.md, and tasks.md to address a reported bug — adds missing requirements, fixes conflicts, reopens false completions, and adds new tasks. Minimal changes only, never regenerates from scratch.

## User Input

```text
$ARGUMENTS
```

ALL outputs MUST be in Russian.

You **MUST** consider the user input before proceeding (if not empty). The user may specify a bug report to patch (e.g., "BUG-001") or describe the fix directly.

## Prerequisites

1. Verify a spec-kit project exists by checking for `.specify/` directory
2. Locate the current feature's spec directory
3. Check for bug reports in `specs/{feature}/bugs/` — if a bug ID is provided, load that report
4. If no bug report exists, inform the user and suggest running `/speckit.bugfix.report` first

## Outline

1. **Load bug context**: Read the relevant bug report and all spec artifacts:
   - **Bug report**: `specs/{feature}/bugs/BUG-{NNN}.md` (if specified)
   - **Required**: `spec.md`, and at least one of `plan.md` or `tasks.md`
   - **Optional**: `research.md`, `data-model.md`

2. **Determine patches**: Based on the bug type, plan minimal changes:

   | Bug Type | spec.md Patch | plan.md Patch | tasks.md Patch |
   |----------|--------------|---------------|----------------|
   | Spec gap | Add missing requirement to affected user story | Add implementation note to relevant section | Add new task(s) for the missing requirement |
   | Spec conflict | Resolve conflict with strikethrough on superseded text + new clarified requirement | Update affected section | Update affected task descriptions |
   | Implementation drift | Add clarification note to requirement | No change (plan was correct) | Reopen drifted task with correction note |
   | Untested flow | Add success criterion for the edge case | Add edge case to complexity tracking | Add verification task |
   | Dependency issue | Update assumption about external dependency | Update technical context | Add dependency investigation task |

3. **Patch spec.md**:
   - Add missing requirements under the affected user story
   - Mark conflicting text with `~~strikethrough~~` and reason
   - Add success criteria for untested flows
   - Update assumptions if dependencies changed
   - Add a bugfix note:
     ```
     **Bugfix**: [DATE] — [BUG-NNN] [Brief description of what was patched]
     ```

4. **Patch plan.md** (if it exists):
   - Update affected sections with new context
   - Add complexity notes for newly discovered edge cases
   - Preserve all existing content — only add or annotate
   - Add a bugfix note:
     ```
     **Bugfix**: [DATE] — [BUG-NNN] Updated from bugfix patch
     ```

5. **Patch tasks.md** (if it exists):
   - **Add new tasks**: Assign next sequential IDs, proper dependencies, and story labels
   - **Reopen tasks**: Change `[x]` back to `[ ]` with a note: `(reopened — BUG-NNN)`
   - **Mark false completions**: Add `⚠️ Reopened` prefix to task description
   - **Update Wave DAG**: If present, regenerate to include new tasks
   - Add a bugfix note:
     ```
     **Bugfix**: [DATE] — [BUG-NNN] Updated from bugfix patch
     ```

6. **Update bug report**: Mark the bug report file as patched:
   ```
   **Status**: Patched
   **Patched**: [DATE]
   ```

7. **Report**: Output a summary:
   - What changed in each artifact
   - How many requirements were added or updated
   - How many tasks were added or reopened
   - Suggest next step: `/speckit.bugfix.verify` to confirm consistency, then `/speckit.implement` to apply the code fix

## Rules

- **Surgical updates only** — never regenerate artifacts from scratch, only modify affected sections
- **Never delete content** — use strikethrough for superseded text, preserve history
- **Preserve formatting** — match existing artifact style exactly
- **Track changes** — always add bugfix notes with dates and bug IDs
- **Reopen, don't delete tasks** — falsely completed tasks get reopened, not removed
- **Require bug report** — if no bug report or user description is provided, refuse to patch and suggest `/speckit.bugfix.report` first
- **Minimal changes** — change only what is necessary to address the specific bug