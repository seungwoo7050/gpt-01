# Universal Guide for Legacy Project Reconstruction & Documentation

## 1. Core Philosophy & Objective

This document outlines a universal, systematic methodology for transforming an unreliable legacy codebase into a robust, maintainable, and professionally documented project. 

The **Core Objective** is to create a verifiable, step-by-step development history by reconstructing the project in logical, test-backed increments (MVPs). This process builds trust in the code and produces high-quality documentation explaining the technical decisions and architecture.

**The Single Source of Truth:**
- **Code:** The newly written, verified source code in the `[project-name]/` directory is the only source of truth for the current state of the application.
- **Specification:** The `DevHistory_MVP_X.md` documents are the official specifications for each incremental stage of the project, 100% synchronized with the code.

---

## 2. Phase 0: Project Initialization

This phase establishes the workspace structure. The original legacy project must be treated as a **read-only reference** and should not be modified.

1.  **Create Reconstruction Root:** Create a new top-level directory for the reconstruction effort.

2.  **Create Subdirectories:** Inside the new root directory, create two subdirectories:
    ```
    Reconstruction_Project/
    ├── legacy_project/         (Read-only reference)
    ├── [project-name]/         (The new, clean codebase will be built here)
    └── devhistory/
        └── Project_Rule.md (This document)
    ```

3.  **Initialize Rules:** Place this document inside the `devhistory/` directory.

---

## 3. Core Workflow: The MVP-based Reconstruction Cycle

This iterative cycle is the heart of the process. It is repeated for each MVP, from 1 to N.

### 3.1. Goal Identification (MVP-N)
- **Action:** Analyze the legacy `DEVELOPMENT_JOURNEY.md` document to understand the intended scope of the current MVP.
- **Task:** Define a single, clear, and concise goal for the MVP (e.g., "Implement a data-oriented ECS framework," "Secure the network layer with TLS").

### 3.2. Code & Asset Discovery
- **Action:** Using file search tools (`glob`, `search_file_content`), locate all files and code snippets from the **legacy project** that are relevant to the MVP's goal.
- **Task:** Compile a list of all relevant legacy assets that will be reconstructed or referenced.

### 3.3. Logical Sequencing Plan
- **Action:** Analyze the discovered legacy assets and their dependencies.
- **Task:** Define and document a logical, step-by-step implementation plan.

### 3.4. Execution: Code Reconstruction & Verification
This is the primary implementation step. All work is done within the new `[project-name]/` directory.

- **Sequence Annotation Principles:**
    - **Initial Annotation:** When encountering code with only legacy numeric comments (`// [SEQUENCE: 123]`), the old comments are **removed** and **replaced** with the new `// [SEQUENCE: MVP-N-X]` format.
    - **Historical Annotation:** When encountering code that **already has** a comment from a previous MVP (e.g., `// [SEQUENCE: MVP-A-B]`), the old comment **must be preserved**. The new `// [SEQUENCE: MVP-N-X]` comment for the current work should be **added directly below it**. This creates a chronological log of changes on that block of code.

- **Code Modification Principles:**
    - **Improve, Don't Just Copy:** The goal is to improve the legacy code. This includes refactoring for clarity, fixing bugs, and applying modern design patterns.
    - **Preserve, Don't Delete:** When a feature is deprecated or replaced, the old code should be **commented out** with an explanatory note rather than being deleted. This preserves context.

- **The Principle of Trust through Testing (TDD):**
    - Since the legacy code has no tests, its correctness is unproven. Trust is established via testing.
    - For every significant unit of logic, a corresponding **unit test must be written**.
    - The preferred workflow is to first write a **failing test** that precisely defines the requirements, then write the implementation code to make that test pass.

### 3.5. `DevHistory` Document Regeneration
- **Action:** Once the code for MVP-N is complete, tested, and re-sequenced, create the `devhistory/DevHistory_MVP_N.md` document **from scratch**.
- **Task:** This document must be based **only on the new, verified code**. It must contain three key sections in order:
    1.  **Introduction:** A high-level overview explaining the "why" of the MVP and its goals.
    2.  **In-depth Analysis for Technical Interviews:** A detailed section explaining architectural choices, technology trade-offs, challenges faced, and potential **Production Considerations** or future improvements.
    3.  **Sequence List:** The complete, ordered list of all `[SEQUENCE: MVP-N-X]` markers for the MVP.

### 3.6. Final Verification (Cumulative Build)
- **Action:** Each MVP is officially completed only after a successful cumulative build and test run.
- **Task:** Perform a full, **cumulative build** of the entire project up to the current MVP. Run **all unit tests** from all completed MVPs to guarantee no regressions. This entire process, including any errors encountered and their resolutions, must be documented in a final **"Build Verification"** section in the `DevHistory_MVP_N.md` file.

---

## 4. General Principles & Procedures

### 4.1. Build Error Resolution Procedure
When a build fails, address it systematically:
1.  **Analyze:** Read the compiler/linker error message carefully to identify the root cause.
2.  **Isolate:** If necessary, try to reproduce the error in a minimal example to narrow down the problem.
3.  **Check Dependencies:** Review `conanfile.txt` and `CMakeLists.txt` (or `build.gradle`, etc.) to ensure libraries are correctly configured and linked.
4.  **Solve Incrementally:** Fix one error at a time and attempt to rebuild.

### 4.2. Adaptation to Legacy State
- **If a `DEVELOPMENT_JOURNEY.md` is missing:** The scope of an MVP must be inferred from file names, code comments, and logical dependencies.
- **If sequence comments are missing:** A logical sequence must be created from scratch based on an analysis of the code.
- **If code is missing:** The code must be generated from scratch based on the context of the surrounding application and the stated goal of the MVP.