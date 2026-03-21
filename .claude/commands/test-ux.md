---
description: Simulates a user going through the module to find gaps in instructions or missing code.
argument-hint: [module_directory]
---
Pass the directory "$1" to the @tester subagent.
Instruct the @tester to use the `ux-review` skill to evaluate the README and the code in "$1". The tester should output a report of missing steps and confusing instructions.
