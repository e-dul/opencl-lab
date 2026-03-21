---
description: Audits a module's README and Design doc, verifying facts via web search and removing redundancies.
argument-hint: [module_name]
---
Pass the context "$1" to the @auditor subagent.
Instruct the @auditor to use the `technical-audit` skill to review "$1/README.md" and "workflow/design/design_$1.md". The auditor must use web search to validate technical claims and identify redundancies.
