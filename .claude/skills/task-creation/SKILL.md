---
name: task-creation
description: Standard procedure for creating atomic execution tasks from a design document.
allowed-tools: Read, FileEdit
---
# Task Generation Skill

You are executing the Tactical Planning phase of the workflow.

## Instructions
1. **Fetch Context:** Read the specific module's design document (e.g., `workflow/design/design_[module].md`) to identify the next unchecked milestone.
2. **Fetch Template:** Read the `workflow/templates/task_doc_template.md` file from the workspace root.
3. **Draft the Task:** Fill out the template for the specific milestone.
4. **Enforce Rules:**
   - **Single Responsibility:** The task must cover exactly one logical feature.
   - **Minimal Context:** Link only to the necessary files (e.g., `common/openclutils.hpp`) and the design doc.
   - **Strict DoD:** The Definition of Done MUST include a verifiable outcome (e.g., `cmake --build .` passes, specific visual output generated).
5. **Save:** Write the output to `workflow/tasks/[id]_[short_name].md` (e.g., `workflow/tasks/001_visual_kernel.md`).
