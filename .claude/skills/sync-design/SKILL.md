---
name: sync-design
description: Procedure for reverse-syncing architectural changes from a completed/blocked task back into the Design document.
allowed-tools: Read, FileEdit
---
# Sync Design (Reverse Update) Skill

## Instructions
1. **Read the Task**: Read the specified task file in `workflow/tasks/`. Look closely at the "Execution Report", "Status", and any notes about blockers or required architectural changes.
2. **Read the Design**: Read the parent `workflow/design/design_[module].md`.
3. **Apply the Pivot/Evolution**: 
   - If the task required a change in architecture, update the "Key Decisions and Rationale" section in the Design doc.
   - If the task failed due to a limitation, add it to the "Known Issues & Risks" section.
   - Update the "Roadmap & Status" checkboxes accordingly.
4. **Save**: Write the changes securely to the Design document.
