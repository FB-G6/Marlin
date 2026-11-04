---
description: Switch to a different bugfix context
---


<!-- Extension: bugfix -->
<!-- Config: .specify/extensions/bugfix/ -->
# Switch bugfix context

Update current context into existing feature. The feature identity is defined by the user.

## User Input

```text
$ARGUMENTS
```

ALL outputs MUST be in Russian.

## Outline

1. **Validate feature**:
   - Check `specs/{feature}` dir exists, report an error to user if missing.

2. **Checkout feature branch**:
   - Checkout a new `{feature}` branch using git command, always use the repository
     default branch `2.1.2` as source.
   - If current branch contains uncommitted changes STOP immediately and escalate to
     project maintainers for guidance.

3. **Feature context update**:
   - Update `.specify/feature.json` with:
     ```json
     {
       "feature_directory": "specs/{feature}"
     }
     ```

4. **Agent context update**:
   - If `AGENTS.md` contains the `<!-- SPECKIT START -->` and `<!-- SPECKIT END -->`
     markers, update the plan reference between them to point to
     `specs/{feature}/plan.md`.
   - If the markers are absent, do not edit `AGENTS.md`: report that the project keeps
     its agent instructions hand-written and that the feature is tracked through
     `.specify/feature.json` only.
