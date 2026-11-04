# Attribution: clean-code-skills

The skills in this directory are adapted from
[ertugrul-dmr/clean-code-skills](https://github.com/ertugrul-dmr/clean-code-skills),
which encodes Robert C. Martin's *Clean Code* rules (MIT License).

Adaptation for this repository:

- TypeScript/Python examples rewritten for C++ and the Marlin firmware style.
- Rule IDs (C, E, F, G, N, T) preserved for traceability.
- Added C++-specific rules CPP1-CPP6 (static memory, const-correctness,
  macros, fixed-width types, no exceptions/RTTI, flash/RAM discipline).
- Added Marlin-specific notes (naming, `ENABLED()`, `F()`, `PROGMEM`,
  build/test commands, `build_params.h` variants).

Upstream license:

```text
MIT License

Copyright (c) 2025

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The upstream README warns to install only one language track per skills
directory because skill names collide. Here there is a single C++ track, so
`boy-scout`, `clean-comments`, `clean-functions`, `clean-general`,
`clean-names`, and `clean-tests` have no duplicates.

To update from upstream:

```bash
git clone --depth 1 https://github.com/ertugrul-dmr/clean-code-skills.git /tmp/clean-code-skills
# review skills/typescript/* and skills/python/*, then re-apply the C++
# adaptations to .opencode/skills/*/SKILL.md
```
