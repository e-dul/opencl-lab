---
description: Updates the Design document based on blockers, notes, or execution reports in a specific Task file.
argument-hint: [task_file_name]
---
Pass the task file "workflow/tasks/$1" to the @architect subagent.
Instruct the @architect to use the `sync-design` skill. It must extract the technical findings or blockers from "$1" and update the parent Design document to reflect this new reality.
