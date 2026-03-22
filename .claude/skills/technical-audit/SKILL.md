---
name: technical-audit
description: Procedure for auditing documentation for factual accuracy and redundancy.
allowed-tools: Read, Grep, web_search, web_fetch, FileEdit
---
# Technical Audit Skill

## Instructions
0. **Big picture**: Read `workflow/design/00-executive-summary.md` to understand vision, philosophy and structure
1. **Analyze the Content**: Read the module's `*.md` then its corresponding `workflow/design/design_[module].md` and code `.h`/`.cpp`/`.cl`/ `CMakeLists.txt`
2. **Fact Check**: 
   - Identify technical claims (e.g., "Nvidia has poor OpenCL 2.0 support", "stb_image doesn't support 16-bit PNGs").
   - Use the `web_search` tool to query the internet and validate these claims against current data.
3. **De-duplicate**: Identify areas where README or code comments redundantly explain the exact same concept. 
4. **Report & Fix**: 
   - Output an "Audit Report" listing false claims (with citations from your web search) and redundancies.
   - Use `FileEdit` to strip redundant comments from the files if necessary.
5. **Suggest Additional Materials**:
   - Suggest external knowlegde sources to explor specific topics in depth
   - Use the `web_search` tool to provide references 
6. **Suggest Updates**:
   - Improving consistency between design to code
   - Clearly mark HW requirements and limitation if missing in README