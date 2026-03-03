---
description: Builds, runs, and validates the implementation against the task's Definition of Done, then fills in the Execution Report.
argument-hint: [task_file_name]
---
Pass the file path "workflow/tasks/$1" to the @coder subagent.

Instruct the @coder to use the `validate-dod` skill to validate the implementation required by the task "$1". The @coder MUST NOT modify any source files — only the `## Definition of Done` checkboxes and the `## Execution Report` section of the task file may be updated.
