---
description: Automatically generates a module design document using the @architect subagent.
argument-hint: [module-name]
---
Pass the context "$1" to the @architect subagent. 

Instruct the @architect to use the `design-creation` skill to generate a complete design document for the "$1" module. The @architect should read the Executive Summary for context and save the final file in the `workflow/design/` directory.
