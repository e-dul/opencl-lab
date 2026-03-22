---
name: design-creation
description: Standard operating procedure for creating Applied OpenCL Lab design documents. Use this skill whenever a new module design or architecture document is requested.
allowed-tools: Read, Grep, FileEdit, Bash
---
# Design Document Creation Skill

You are executing the Strategy phase of the Applied OpenCL Lab workflow.

## Instructions
1. **Read existing**: Read design `workflow/design/design_[module_name].md` if it's available.
2. **Gather Context**: Read `workflow/design/00-executive-summary.md` and `rules/00_master_specs.md` to understand the project philosophy and the specific module's goals.
3. **Fetch Template**: Read the `workflow/templates/design_doc_template.md` file from the workspace to get the exact markdown structure required.
4. **Draft the Design**: Fill out the template for the requested module. 
5. **Enforce Rules**: 
   - NEVER include low-level execution logs, `.cpp` implementation details, or task definitions.
   - Keep the high-level status purely to Phases/Milestones (checkboxes).
   - Ensure performance gates are clearly defined based on the Executive Summary.
6. **Save**: Write the final output to `workflow/design/design_[module_name].md`.
