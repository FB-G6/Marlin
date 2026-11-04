---
description: Capture a bug and trace it back to the relevant spec, plan, and task artifacts
---


<!-- Extension: bugfix -->
<!-- Config: .specify/extensions/bugfix/ -->
# Report Bug

Capture a bug discovered during implementation and trace it back to the relevant specification artifacts. Produces a structured bug report that maps the issue to spec requirements, plan sections, and tasks.

## User Input

```text
$ARGUMENTS
```

ALL outputs MUST be in Russian.

You **MUST** consider the user input before proceeding (if not empty). The user describes the bug — what went wrong, error messages, unexpected behavior, or a gap discovered during implementation.

## Prerequisites

1. Verify a spec-kit project exists by checking for `.specify/` directory
2. Locate the current feature's spec directory (by branch name or most recently modified)
3. Verify at least `spec.md` exists

## Outline

1. **Load artifacts**: Read from the current feature directory:
   - **Required**: `spec.md` (the specification)
   - **Optional**: `plan.md`, `tasks.md`, `research.md`, `data-model.md`

2. **Analyze the bug**: From the user's description, classify the bug:

   | Bug Type | Description | Example |
   |----------|-------------|---------|
   | Spec gap | Requirement missing from spec | Auth flow doesn't handle expired tokens |
   | Spec conflict | Two requirements contradict | "Must be stateless" vs "Must track sessions" |
   | Implementation drift | Code diverges from spec | Spec says REST, code uses GraphQL |
   | Untested flow | Edge case not covered in success criteria | Concurrent user updates not handled |
   | Dependency issue | External dependency behaves differently than assumed | API response format changed |

3. **Trace to artifacts**: Map the bug to specific sections in each artifact:
   - **spec.md**: Which user story, requirement, or success criterion is affected?
   - **plan.md**: Which plan section covers this area?
   - **tasks.md**: Which task(s) relate to this area? Are any marked complete that shouldn't be?

4. **Generate bug report**: Output a structured report:

   ```markdown
   # Bug Report: [Short Title]

   **Type**: [Spec gap | Spec conflict | Implementation drift | Untested flow | Dependency issue]
   **Severity**: [Critical | High | Medium | Low]
   **Feature**: [Feature name/branch]
   **Reported**: [DATE]

   ## Description
   [User's bug description, clarified and structured]

   ## Artifact Traceability

   ### spec.md
   - **Affected user story**: [Story N — title]
   - **Affected requirements**: [List specific requirements]
   - **Gap identified**: [What is missing or wrong in the spec]

   ### plan.md
   - **Affected sections**: [List plan sections]
   - **Impact**: [What needs to change in the plan]

   ### tasks.md
   - **Affected tasks**: [Task IDs and descriptions]
   - **False completions**: [Tasks marked done that need reopening]
   - **Missing tasks**: [New tasks needed to fix the bug]

   ## Root Cause Analysis
   [Why this bug exists — was it a spec oversight, changed requirement, or implementation error?]

   ## Recommended Fix
   1. Run `/speckit.bugfix.patch` to update spec artifacts
   2. Run `/speckit.bugfix.verify` to confirm consistency
   3. Resume `/speckit.implement` to apply the code fix
   ```

5. **Save report**: Write the bug report to `specs/{feature}/bugs/BUG-{NNN}.md` where `{NNN}` is the next sequential bug number. Create the `bugs/` directory if it does not exist.

6. **Report**: Output the bug report and suggest next steps.

## Rules

- **Always trace to artifacts** — every bug must map to at least one spec section
- **Never modify spec artifacts** — this command only reports, use `/speckit.bugfix.patch` to make changes
- **Sequential numbering** — bug reports are numbered BUG-001, BUG-002, etc.
- **Classify accurately** — distinguish between spec gaps (missing requirements) and implementation drift (code doesn't match spec)
- **Be specific** — reference exact user story numbers, requirement text, and task IDs