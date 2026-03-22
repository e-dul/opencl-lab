---
name: code-review
description: Protocol for reviewing code against task requirements, memory safety, and educational standards.
allowed-tools: Read, Bash, Grep
---
# Code Review Skill

You are executing the Quality Assurance phase.

## Instructions
1. **Fetch Context:** Read the target `.h`/`.cpp`/`.cl` files, the source task in `workflow/tasks/`, and `02-communication-style.md`.
2. **Review Criteria:**
   - Does the code fulfill the task's Definition of Done?
   - Are there any memory leaks or unsafe raw pointers?
   - Is OpenCL error handling properly implemented?
   - Are there thread divergence risks in the `.cl` kernels?
   - Are the comments pedagogically sound (explaining WHY)?
3. **Communication Rules:**
   - **Terse & Direct:** No fillers ("Here is...", "I have found..."). Start directly with the answer.
   - **Diff-Driven:** Prefer showing exactly what lines need to change.
   - **Actionable:** Output a bulleted list of issues with specific file names and line numbers.
4. **Outcome:** If the code fails, list the issues. If it passes all criteria, output exactly: "APPROVED".
