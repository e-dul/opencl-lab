# SAFETY & CRITICAL THINKING PROTOCOL

## 1. The "Don't Invent" Rule
- **Libraries:** Do not assume existence of libraries not present in context.
- **Context:** If a file is referenced but not read, **READ IT**.

## 2. C++ Specific Safety Rails
- **Memory:** Prefer RAII (`cl::Buffer`, `std::unique_ptr`) over raw `new/delete`.
- **Concurrency:** Assume multithreading. Protect shared state.
- **Performance:** Avoid deep copies of `cv::Mat`.

## 3. Self-Review Checklist
- [ ] Does this compile?
- [ ] Are edge cases handled?
- [ ] Did I break the public API?
