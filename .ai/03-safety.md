# SAFETY & CRITICAL THINKING PROTOCOL

## 1. The "Don't Invent" Rule
- **Libraries:** Do not assume existence of libraries or functions not present in the provided context or standard C++ documentation.
- **Context:** If a file is referenced but not read, **READ IT** before making assumptions about its contents.
- **Uncertainty:** If user requirements are ambiguous (e.g., missing platform constraints, specific OpenCV version), **ASK** clarifying questions before generating code.

## 2. Two-Step Thinking Process
Before generating implementation code, perform an internal "Analysis Step":
1. **Analyze Requirements:** Restate the core problem in 1 sentence.
2. **Identify Risks:** (Thread safety, memory leaks, expensive copies in CV pipeline).
3. **Plan:** Outline the solution steps.
Only then generate the code.

## 3. C++ Specific Safety Rails
- **Memory:** Prefer RAII (`cl::Buffer`, `std::unique_ptr`) over raw `new/delete`.
- **Concurrency:** Assume multithreading. Protect shared state.
- **Performance:** Avoid deep copies of `cv::Mat`.

## 4. Self-Review Checklist
After generating code, verify against:
- [ ] Does this compile? (Syntax check)
- [ ] Are all new dependencies declared?
- [ ] Are edge cases handled? (Empty images, null pointers, network disconnects)
- [ ] Did I break the public API contract?

## 5. Handling Conflicts
If the requested Task contradicts the Design document (`design/*.md`):
- **STOP immediately.**
- Inform the user about the conflict.
- **Do not** implement code that violates the approved architecture without explicit confirmation (Reverse Engineering Mode).

