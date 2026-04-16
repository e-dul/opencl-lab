# Capability: safety

Source: `03-safety.md` §1–§4

## Overview

AI-generated code must not invent libraries, must validate assumptions before writing, and must pass a self-review checklist before any code is proposed.

---

#### Scenario: Don't invent libraries

WHEN code is generated that uses a library or function
THEN that library or function must be present in the provided context or standard C++ documentation
AND assumed existence of undeclared dependencies is forbidden

#### Scenario: Read before assuming

WHEN a file is referenced in a task or prompt
THEN the file is read before making assumptions about its contents

#### Scenario: Clarify ambiguous requirements

WHEN user requirements are ambiguous (missing platform constraints, version specifics, etc.)
THEN clarifying questions are asked before implementation begins
AND code generation does not proceed on ambiguous inputs

#### Scenario: Two-step thinking before code

WHEN implementation code is about to be generated
THEN an internal analysis step is performed first:
  1. Restate the core problem in one sentence
  2. Identify risks (thread safety, memory leaks, expensive copies)
  3. Outline solution steps
THEN code generation proceeds

#### Scenario: RAII memory management

WHEN C++ objects managing resources are designed
THEN RAII patterns (`cl::Buffer`, `std::unique_ptr`) are used
AND raw `new/delete` is not used for resource management

#### Scenario: Concurrency assumption

WHEN code accesses shared state
THEN multithreading is assumed
AND shared state is protected appropriately

#### Scenario: Self-review before proposing code

WHEN implementation code is proposed
THEN it is verified against:
  - Syntax is correct and would compile
  - All new dependencies are declared
  - Edge cases are handled (empty images, null pointers)
  - The public API contract is not broken

#### Scenario: Conflict with approved specs

WHEN a requested task contradicts an existing spec in `.openspec/specs/`
THEN implementation stops immediately
AND the conflict is reported to the user before proceeding
