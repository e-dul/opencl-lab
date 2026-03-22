---
description: Evaluates a module's README and code, assigning a 1-10 grade across multiple educational and technical criteria.
argument-hint: [module_directory]
---
Pass the directory "$1" to the @evaluator subagent.
Instruct the @evaluator to use the `grading` skill to comprehensively review the README and source code inside "$1". The evaluator must output a formal grading report with a final summarized score. Use `Strict Output Template` to be consistent.
