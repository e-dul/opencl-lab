---
description: Implements C++/OpenCL code for a specific task using the @coder subagent.
argument-hint: [task_file_name]
---
Pass the file path "workflow/tasks/$1" to the @coder subagent. 

Instruct the @coder to use the `cpp-opencl-dev` skill to strictly implement the code required by the task "$1". The @coder MUST NOT modify any design documents and MUST run `cmake --build .` to ensure the implementation compiles successfully before finishing.
