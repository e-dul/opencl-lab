# AI Engineering Guidelines for Applied OpenCL Lab

## Project Philosophy
1. **Code-First:** Code is the primary knowledge carrier.
2. **Just-in-Time Learning:** Theory on demand.
3. **Education First:** Code must be readable by Mid-level engineers.
4. **Performance:** Always profile (Events) before optimizing.

## Communication Style
- Terse: No filler phrases.
- Diff-Driven: Show code changes.
- Direct: Start with the answer.

## Other
- Consider task completed only after explicitly told.

## OpenSpec Workflow
- Use `/opsx:new` to propose any new feature or significant change before writing code.
- Specs live in `openspec/specs/`; active changes in `openspec/changes/`.
- State machine: proposal → specs → design → tasks → archive (`/opsx:archive`).
- Never skip the proposal phase for non-trivial work.
- Never implement unless explicitly told to proceed after proposal is approved.

## Discipline
- NEVER use sed or custom python scripts for file modifications
- ALWAYS show a clear diff before applying any change
- Use str_replace with explicit before/after blocks
- DO NOT USE /tmp dir