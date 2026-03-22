---
name: readme-creation
description: Procedure for writing educational, user-facing Module READMEs.
allowed-tools: Read, FileEdit
---
# README Creation Skill

You are executing the Documentation/Educational phase.

## Instructions
1. **Read Existing Readme:** Read file `[ModuleFolder]/[Module Name].md` if it's available. See `02_Projects/A_Multimedia/Multimedia.md` or `99_Toolbox/CoalescedAccess/CoalescedAccess.md` for samples. 
2. **Fetch Context:** Read `00-executive-summary.md` to understand the overarching goal of the track/module.
3. **Fetch Template:** Read the `workflow/templates/module_doc_template.md` file from the workspace root.
4. **Draft the README:** 
   - Write 1-2 sentences explaining what the module teaches and why it matters.
   - Define exact terminal commands (`bash cd 01FolderName`, `cmake -B build`, `cmake --build build`) for the user to run.
   - Define the expected visual or terminal output so the user can verify success.
5. **Pedagogy Rules:**
   - **Just-in-Time Learning:** Only introduce theory (e.g., Memory Coalescing, Thread Divergence) *after* the user has run the code or encountered the problem.
   - Add a small "Mini-challenge" for the user to modify the code and test their understanding.
6. **Save:** Write the output to `[ModuleFolder]/[Module Name].md`. See `02_Projects/A_Multimedia/Multimedia.md` or `99_Toolbox/CoalescedAccess/CoalescedAccess.md` for samples. 
