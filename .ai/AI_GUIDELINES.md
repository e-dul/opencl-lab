# AI Engineering Guidelines for Applied OpenCL Lab

## Project Philosophy
1. **Code-First:** Code is the primary knowledge carrier.
2. **Just-in-Time Learning:** Theory on demand.
3. **Education First:** Code must be readable by Mid-level engineers.
4. **Performance:** Always profile (Events) before optimizing.

## Architecture
- `common/` contains helper functions (error handling, image IO).
- Kernels (`.cl`) are separate files, loaded at runtime.
- Build system is CMake (standalone per module).
- Use RAII wrappers for OpenCL resources.

## Code Conventions
- Variables: `snake_case`
- Classes: `PascalCase`
- Constants: `UPPER_CASE`
- Always check OpenCL return codes.

## Communication Style
- Terse: No filler phrases.
- Diff-Driven: Show code changes.
- Direct: Start with the answer.
