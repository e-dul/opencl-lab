# Task 043: Add-on 4.2 — OpenCL vs CUDA Written Analysis

## Context
- **Design Feature:** `workflow/design/08-addons.md`
- **Milestone:** Phase 2 — 4.2 OpenCL vs CUDA (written analysis artifact; no binary)
- **Relevant Files:**
  - `workflow/design/08-addons.md` — (read-only: source of truth; §4.2 Exception: written analysis only)
  - `04_Addons/4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md` — (read-only: README / educational context)
  - `04_Addons/4_2_OpenCL_vs_CUDA/report.md` — (new file: primary deliverable)
  - `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cu` — (new file)
  - `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cl` — (new file)

## Objective

Produce the structured written analysis artifact for add-on 4.2: a `report.md` covering the OpenCL vs CUDA ecosystem comparison, a decision matrix, side-by-side `vector_add` code samples, and a pragmatic recommendation — with no binary required.

## Constraints & Rules

- **No binary.** The design explicitly designates 4.2 as a written-analysis-only add-on. Do NOT produce a CMakeLists.txt, `main.cpp`, or any compiled artifact.
- **Design Wins conflict note:** `04_Addons/4_2_OpenCL_vs_CUDA/OpenCLvsCUDA.md` describes a `portability_demo` binary. This contradicts the design doc. Per the "Design Wins" rule, no binary is produced. The README may be updated in Phase 8 (module cleanup), not here.
- `report.md` must be self-contained — readable without building anything.
- Code samples (`vector_add.cu` / `vector_add.cl`) must be syntactically valid, minimal, and cover the same problem (element-wise float addition, 1D global index) so the comparison is fair.
- No hardcoded asset paths; no CLI11 required (no binary).

---

## Implementation

1. Create `04_Addons/4_2_OpenCL_vs_CUDA/report.md` with the following mandatory sections:
   - **Ecosystem Comparison Table** — columns: Dimension | OpenCL | CUDA. Rows must include: Vendor Support, Language, Tooling, AI/ML Ecosystem, Embedded/FPGA Support, Community & Resources, Driver Complexity.
   - **Use-Case Decision Matrix** — columns: Scenario | Recommended API | Rationale. Minimum 6 rows covering: new AI/ML project, FPGA target, multi-vendor deployment, mobile/embedded, Nvidia-only HPC, open-source desktop application.
   - **Side-by-Side Code Reference** — inline fenced code blocks showing `vector_add.cu` and `vector_add.cl` in adjacent sections. Annotate structural differences (host setup, kernel launch syntax, memory model).
   - **Pragmatic Recommendation** — 2–4 sentences; opinionated, grounded in the comparison table, not a disclaimer.
   - **Mini-Challenge** — one concrete experiment referencing the code comparison files.

2. Create `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cu` — minimal CUDA C++ vector addition: `cudaMalloc`, `cudaMemcpy`, `__global__` kernel, `<<<grid, block>>>` launch syntax, `cudaFree`. Target CUDA 11+.

3. Create `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cl` — equivalent OpenCL kernel (`.cl` only, no host code). Single `__kernel void vector_add(...)` with `get_global_id(0)` guard. This is the kernel file only; host code lives conceptually in `report.md`'s annotated walkthrough.

   - Optional: add a `vector_add_host.cpp` stub (no CMakeLists) showing the OpenCL host side if it significantly aids the comparison narrative. Only if it adds clarity.

## Definition of Done (DoD)

<!-- Standard DoD from §8 of master_specs applies where relevant. No build gate (no binary). -->

- [x] `04_Addons/4_2_OpenCL_vs_CUDA/report.md` exists and is non-empty.
- [x] `report.md` contains all five mandatory sections: Ecosystem Comparison Table, Use-Case Decision Matrix, Side-by-Side Code Reference, Pragmatic Recommendation, Mini-Challenge.
- [x] Ecosystem Comparison Table has at minimum 7 rows covering the dimensions listed above.
- [x] Decision Matrix has at minimum 6 scenario rows.
- [x] `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cu` exists and is syntactically valid CUDA C++ (manually verifiable by inspection).
- [x] `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cl` exists and is syntactically valid OpenCL C kernel (manually verifiable by inspection).
- [x] Both code samples solve the identical problem (element-wise float addition) with the same array size parameter.
- [x] No CMakeLists.txt or compiled binary artifact is produced in the 4.2 directory.
- [x] MANUAL: Read `report.md` end-to-end; confirm the Pragmatic Recommendation is opinionated (picks a clear winner for each scenario) rather than a neutral disclaimer.
- [x] MANUAL: Review `vector_add.cu` and `vector_add.cl`; confirm they are structurally parallel (same algorithm, equivalent memory operations) and the annotations in `report.md` correctly identify the differences.

---

## Execution Report
<!-- Filled by @coder after implementation. -->

- **Status:** DONE
- **Session:** 2026-03-19

### Validation
```
[PASS] report.md exists and is non-empty (168 lines).
[PASS] All five mandatory sections present:
       - §1 Ecosystem Comparison Table
       - §2 Use-Case Decision Matrix
       - §3 Side-by-Side Code Reference
       - §4 Pragmatic Recommendation
       - §5 Mini-Challenge
[PASS] Ecosystem Comparison Table: 8 rows (>= 7 required).
       Rows: Vendor Support, Language, Tooling, AI/ML Ecosystem,
             Embedded/FPGA Support, Community & Resources,
             Driver Complexity, Portability.
[PASS] Decision Matrix: 7 scenario rows (>= 6 required).
[PASS] vector_add.cu exists; syntactically valid CUDA C++:
       __global__ kernel, <<<GRID,BLOCK>>> launch, cudaMalloc/cudaFree,
       N = 1<<20, int n guard.
[PASS] vector_add.cl exists; syntactically valid OpenCL C kernel:
       __kernel, __global qualifiers, get_global_id(0), size_t gid,
       size_t cast guard, same int n parameter.
[PASS] Both files solve c[i]=a[i]+b[i] for N=1<<20 floats with same int n.
[PASS] No CMakeLists.txt or compiled binary in 4.2 directory.
       Directory contents: OpenCLvsCUDA.md, report.md,
       code_comparison/vector_add.cl, code_comparison/vector_add.cu.
[ ] MANUAL: Confirm Pragmatic Recommendation is opinionated (not a neutral disclaimer).
[ ] MANUAL: Confirm vector_add.cu and vector_add.cl are structurally parallel and
            report.md annotations correctly identify differences.
          
```

### Changed Files
| File | Change |
|------|--------|
| `04_Addons/4_2_OpenCL_vs_CUDA/report.md` | Created |
| `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cu` | Created |
| `04_Addons/4_2_OpenCL_vs_CUDA/code_comparison/vector_add.cl` | Created |
