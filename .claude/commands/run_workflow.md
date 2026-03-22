# Run Workflow Pipeline

Run the full workflow pipeline for a task with review/fix loops.

**Arguments:** `$TASK_ID` (required), `$MAX_REVIEW_ITERS` (optional, default: 4)

## Instructions

You are orchestrating the full pipeline for task **$TASK_ID**.
Max review iterations: **$MAX_REVIEW_ITERS** (default 4 if not provided).

Follow these steps **in order**, tracking state as you go:

---

### Step 1 — Implement
Run `/implement` for $TASK_ID.
- If it fails, stop and report failure.

### Step 2 — Review + Fix Loop
Repeat up to $MAX_REVIEW_ITERS times:
1. Run `/review` for $TASK_ID. Then show list of issues.
2. If output contains `APPROVED` → exit loop, proceed to Step 3.
3. If issues found → run `/implement` to fix, then repeat review.

If APPROVED is never reached after $MAX_REVIEW_ITERS iterations → **stop and report failure**.

### Step 3 — Validate
Run `/validate` for $TASK_ID.
- If it fails → **stop immediately, do not run sync**.
- If DoD MANUAL are defined → **stop immediately, do not run sync. Instead show summary of MANUAL DoD**. 

### Step 4 — Sync
Run `/sync` for $TASK_ID.

---

## Final Summary (always print this)

| Field | Value |
|---|---|
| Task | $TASK_ID |
| Result | SUCCESS or FAILED (at which step) |
| Review iterations used | X / $MAX_REVIEW_ITERS |
| Validate | PASSED or FAILED |
| Sync | DONE or SKIPPED |

Summarize review errors that can be used as future development guidelines.