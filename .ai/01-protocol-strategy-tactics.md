# PROJECT OPERATING PROTOCOL: STRATEGY & TACTICS

## 1. Core Philosophy: Dual-Layer Context
To prevent context rot and hallucinations, we separate **Long-term Truth** from **Short-term Execution**.

### 🛡️ STRATEGY (The Design Layer)
- **Location:** `design/<feature_name>.md`
- **Purpose:** Single Source of Truth for Architecture, Vision, Key Decisions, and High-Level Status.
- **Lifecycle:** Long-lived. Updated only upon milestone completion or design changes.
- **Rule:** NEVER implicitly change architecture defined here.

### ⚔️ TACTICS (The Task Layer)
- **Location:** `tasks/<id>_<task_name>.md`
- **Purpose:** Disposable instructions for atomic units of work.
- **Lifecycle:** Ephemeral. Created -> Executed -> Archived (`tasks/archive/`).
- **Rule:** Must strictly follow the Strategy.

---

## 2. The Operating Loop
Follow this cycle for every feature request:

1.  **ANALYZE (Strategy Mode)**
    - Read `design/[FEATURE].md`.
    - Identify the next logical step from the Roadmap.

2.  **PLAN (Tactics Mode)**
    - Create a new file: `tasks/[ID]_[NAME].md`.
    - Copy *minimal* relevant context from Design.
    - Define clear **Definition of Done (DoD)** (e.g., "Builds and passes test X").

3.  **EXECUTE (Coder Mode)**
    - Implement code based **STRICTLY** on the Task file.
    - Do not modify `design/` files during this phase.
    - Do not read archived tasks unless necessary for migration.

4.  **SYNC & CLEAN (Closing)**
    - ✅ Mark status in `design/[FEATURE].md` (check boxes).
    - 📝 Add any "Known Issues" discovered to Design.
    - 🗑️ Move Task file to `tasks/archive/`.

---

## 3. Change Management Protocol

### The "DESIGN WINS" Rule
If Code contradicts Design (`design/*.md`):
1.  **Default:** Design is the Truth. The Code is wrong/outdated.
2.  **Action:** Create a task to fix the code divergence.
3.  **Exception:** Only update Design to match Code if explicitly instructed (Reverse Engineering Mode).

### Evolution vs. Pivot
- **Evolution (Small Change):**
    1. Update `design/` first.
    2. Create a new Task to align code.
- **Pivot (Major Change):**
    1. Re-write `design/` completely (do not just append).
    2. Move conflicting/old tasks to `tasks/archive/legacy/`.
    3. Create a "Migration Task" to clean up old code before writing new code.

---

## 4. Role Definitions

### @architect
- **Focus:** `design/` files, structure, interfaces, decisions.
- **Action:** Reads user intent, updates Design, generates Task files.
- **Constraint:** Does NOT write implementation code (.cpp).

### @coder
- **Focus:** `src/`, `include/`, `tasks/`.
- **Action:** Reads Task file, implements code, runs tests.
- **Constraint:** Does NOT change architecture or decisions without permission.

### @reviewer
- **Focus:** Code quality, safety, performance.
- **Action:** Read-only analysis. Outputs bulleted lists of issues.
- **Constraint:** Does NOT refactor code automatically.
---

## 5. Knowledge Management & Hierarchy

To ensure consistency between educational goals and technical implementation:

### 5.1 The Hierarchy of Truth

1.  **STRATEGY (`design/*.md`)** = **The Law (Source of Truth)**.
    *   Defines **Technical Specs** & **Hard Constraints** (e.g., "Must output BMP").
    *   References READMEs for context but overrides them on technical details.

2.  **CONTEXT (`*/README.md`)** = **The Textbook (User View)**.
    *   Defines **User Experience** & **Educational Goals**.
    *   Abstract description ("Visual Hello World") without implementation mandates.

3.  **TACTICS (`.ai/tasks/*.md`)** = **The Work Order (Execution)**.
    *   Combines Strategy Specs + User Context into executable steps.

### 5.2 The Agent Memory (`.ai/MEMORY.md`)
*   **Purpose:** Long-term context for the Agent (Project decisions, session lessons).
*   **Rule:** Consult this file at the start of every session to align with project history.