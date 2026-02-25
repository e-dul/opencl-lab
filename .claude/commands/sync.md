---
description: Updates the Design document and archives the completed task to prevent context drift.
argument-hint: [task_file_name]
---
Pass the file path "workflow/tasks/$1" to the @architect subagent. 

Instruct the @architect to perform the "Sync & Clean" phase:
1. Read the completed task "$1".
2. Update the corresponding milestone checkboxes and "Known Issues" in the parent `workflow/design/*.md` file.
3. Move the file "$1" from `workflow/tasks/` to `workflow/archive/` using the Bash tool.
