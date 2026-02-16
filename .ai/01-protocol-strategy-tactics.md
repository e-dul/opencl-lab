# PROJECT OPERATING PROTOCOL: STRATEGY & TACTICS

## 1. Core Philosophy: Dual-Layer Context
To prevent context rot and hallucinations, we separate **Long-term Truth** from **Short-term Execution**.

### 🛡️ STRATEGY (The Design Layer)
- **Location:** `design/<feature_name>.md`
- **Purpose:** Single Source of Truth for Architecture, Vision, Key Decisions, and High-Level Status.
- **Rule:** NEVER implicitly change architecture defined here.

### ⚔️ TACTICS (The Task Layer)
- **Location:** `.ai/tasks/<id>_<task_name>.md`
- **Purpose:** Disposable instructions for atomic units of work.
- **Lifecycle:** Created -> Executed -> Archived (`.ai/archive/`).

## 2. The Operating Loop
1. **ANALYZE:** Read `design/*.md`. Identify next step.
2. **PLAN:** Create `.ai/tasks/XXX_name.md`.
3. **EXECUTE:** Implement code strictly based on the task file.
4. **SYNC & CLEAN:** Update design, move task to archive.
