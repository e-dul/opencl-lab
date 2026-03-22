---
description: Reviews the implemented code against the task requirements using the @reviewer subagent.
argument-hint: [task_file_name]
---
Pass the file path "workflow/tasks/$1" to the @reviewer subagent. 

Instruct the @reviewer to use the `code-review` skill to evaluate the codebase against the requirements and Definition of Done in task "$1". The reviewer must output a terse, diff-driven list of issues or the exact word "APPROVED".
