---
name: grading
description: Standardized grading rubric for evaluating OpenCL Lab modules.
allowed-tools: Read, Glob
---
# Course Grading Skill

## Instructions
0. **Big picture**: Read `workflow/design/00-executive-summary.md` to understand vision, philosophy and structure
1. **Analyze Content**: Read the module's `*README*.md` and scan the relevant `.cpp`, `.h`, `.cl`, and `CMakeLists.txt` files.
2. When directory does't have dedicated `.md` check parent directory.
3. **Apply Rubric**: Grade the module on a scale of 1 to 10 for each of the following criteria. 1 is terrible, 10 is perfect.

## Grading Criteria
- **Theory to Application Ratio**: Does the module strike the right balance? (e.g., Is theory introduced "Just-in-Time" to solve a real application problem, or is it a dry theory dump?)
- **Uniqueness**: Does this module offer unique value (e.g., custom ISP, ROS2 integration) compared to standard, generic OpenCL tutorials found online?
- **Hardware Dependency**: Is the code portable? (e.g., Does it gracefully handle OpenCL 1.2 fallbacks, or does it strictly require an Nvidia GPU with OpenCL 2.0 SVM?)
- **Repetitiveness**: Is the material concise, or does the README redundantly state things that are already obvious from the code comments?
- **Code-to-Explanation Clarity**: Do the code comments and README explain the *WHY* behind the architectural choices (RAII, cl.hpp) rather than just stating *WHAT* the code does?
- **Reproducibility**: Is the build process (CMake) entirely self-contained and easy for a student to execute without external environment hell?

## Final Calculation
- Calculate the average of all criteria to provide a **Summarized Score (1-10)** at the bottom of the report.

## Strict Output Template
Output exactly in this Markdown table format:

| Module Name | Theory/App | Uniqueness | HW Dep. | Repetitiveness | Clarity | Reproducibility | **FINAL SCORE** |
|-------------|------------|------------|---------|----------------|---------|-----------------|-----------------|
| [Module 1]  | [x]/10     | [x]/10     | [x]/10  | [x]/10         | [x]/10  | [x]/10          | **[Avg]/10**    |

### Justification

For each criterion, write the Score and a 1-2 sentence short Justification:

- Theory/App (8) — BT.601 math and T-API ordering are placed just-in-time, but Key Concepts section lacks forward references to exact code locations.
- Uniqueness (9) — NV12/YUYV decoding, RemoteTenso
- ....

### Actionable Items
Actionable items to addres potential issues