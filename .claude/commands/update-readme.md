---
description: Updates a module's README to match recent changes in its Design doc.
argument-hint: [module_name]
---
Pass the context "$1" to the @educator subagent.
Instruct the @educator to use the `sync-readme` skill. It must read the updated "workflow/design/design_$1.md" and safely apply the architectural changes to the user-facing "$1/README.md".
