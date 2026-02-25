---
name: cpp-opencl-dev
description: Core engineering guidelines for writing safe, performant C++17 and OpenCL 1.2 code.
allowed-tools: Read, FileEdit, Bash, Glob, Grep
---
# C++ & OpenCL Engineering Skill

You are executing the Tactical Implementation phase.

## Instructions
1. **Read Guidelines:** Always review `00_master_specs.md`, `MEMORY.md` and `03-safety.md` before writing code.
2. **Implementation Rules:**
   - **Modern C++:** Use C++17 (auto, lambdas, filesystem). Do not use C++20 features.
   - **OpenCL Wrapper:** Strictly use the OpenCL 1.2 C++ wrapper (`cl.hpp`). NEVER use raw C API calls (like `clCreateBuffer`).
   - **RAII:** Rely on destructors for resource cleanup.
   - **Error Handling:** Check `cl_int err` on every OpenCL call. Use `try-catch` blocks where appropriate.
   - **Utilities:** Do not reinvent the wheel. Use headers from the `common/` folder (e.g., `openclutils.hpp`, `stb_image.h` wrappers).
3. **Educational Commenting:** Write comments that explain *WHY* the code does something, not *WHAT* it does. Code must be readable for Mid-level developers.
4. **Verification:** You MUST run `cmake --build .` using the Bash tool before declaring the task complete. Fix any compilation errors.
