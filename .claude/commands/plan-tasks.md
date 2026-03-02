---
description: Generates the next atomic execution task based on a module's design doc using the @architect.
argument-hint: [module_name]
---
Pass the context "$1" to the @architect subagent. 

Instruct the @architect to use the `task-generation` skill to read `workflow/design/$1.md` and generate the next logical, atomic task file in the `workflow/tasks/` directory. Ensure the task has a strict Definition of Done (DoD).
