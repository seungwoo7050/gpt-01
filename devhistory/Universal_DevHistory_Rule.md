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
    - `[project-name]/`: This will house the new, clean, and evolving source code. Initially, it can be empty or contain a basic project setup (e.g., `.gitignore`).
    - `devhistory/`: This will house all historical and process-related documentation.

3.  **Place Legacy Code:** Place the original, untouched legacy project folder alongside these new directories for reference.

4.  **Initialize Rules:** Place this document (`Universal_DevHistory_Rule.md`) inside the `devhistory/` directory. It will govern the entire reconstruction process.

**Example Initial Structure:**
```
Reconstruction_Project/
├── legacy_project/         (Read-only reference)
├── my_new_project/         (The new, clean codebase will be built here)
└── devhistory/
    └── Universal_DevHistory_Rule.md
```

---

## 3. Core Workflow: The MVP-based Reconstruction Cycle

This iterative cycle is the heart of the process. It is repeated for each MVP, from 1 to N.

### 3.1. Goal Identification (MVP-N)

- **Action:** Analyze the legacy `DEVELOPMENT_JOURNEY.md` document to understand the intended scope of the current MVP.
- **Task:** Define a single, clear, and concise goal for the MVP (e.g., "Implement a data-oriented ECS framework," "Secure the network layer with TLS").
- **Adaptation Rule:** If a `DEVELOPMENT_JOURNEY.md` does not exist, infer the MVP's scope by analyzing file names, code comments, and the logical dependencies within the legacy code. If the scope is still unclear, consult with the project lead.

### 3.2. Code & Asset Discovery

- **Action:** Using file search tools (`glob`, `search_file_content`), locate all files and code snippets from the **legacy project** that are relevant to the MVP's goal. This includes source code, configuration files, scripts, etc.
- **Task:** Compile a list of all relevant legacy assets that will be reconstructed or referenced.

### 3.3. Logical Sequencing Plan

- **Action:** Analyze the discovered legacy assets and their dependencies.
- **Task:** Define and document a logical, step-by-step implementation plan. The plan should build from the ground up: 
    1.  Build System & Dependencies
    2.  Data Structures & Components
    3.  Core Logic & Systems
    4.  API / Manager Integration
    5.  Application Entry Point Integration
    6.  Tests

### 3.4. Code Refactoring & Synchronization

This is the primary implementation step. All work is done within the new `[project-name]/` directory.

- **The Golden Rule:** All legacy sequence comments (`// [SEQUENCE: 123]`) within the scope of the current MVP must be **removed**. They are replaced by a new, clean, and continuous sequence of comments starting from 1 for each MVP (e.g., `// [SEQUENCE: MVP-N-1]`, `// [SEQUENCE: MVP-N-2]`).

- **Code Improvement:** The goal is **not to simply copy** the legacy code. It must be improved.
    - **Refactor:** Improve readability, efficiency, and maintainability.
    - **Fix:** Correct logical bugs, race conditions, and memory leaks.
    - **Modernize:** Apply modern language features and design patterns where appropriate.
    - **Generate:** If essential code is missing, it must be created from scratch based on the context provided by other files and the MVP goal.

- **Test-Driven Development (TDD):** The reliability of the legacy code is zero. Therefore, trust is built through testing.
    - For each unit of logic, a corresponding unit test should be written.
    - Where possible, write a failing test first that defines the desired behavior, and then write the implementation code to make the test pass.

### 3.5. `DevHistory` Document Regeneration

- **Action:** Once the code for MVP-N is complete, tested, and re-sequenced, create the `devhistory/DevHistory_MVP_N.md` document **from scratch**.
- **Task:** This document must be based **only on the new, verified code**. 
    - The sequence list must be 100% synchronized with the new `[SEQUENCE: MVP-N-X]` comments in the code.
    - The document must include an **Introduction** explaining the "why" of the MVP and its goals.
    - It must include an **In-depth Analysis** section explaining key architectural decisions and trade-offs, suitable for technical review.

### 3.6. Final Verification (Cumulative Build)

This is the most critical step for ensuring project stability.

- **Action:** Perform a full, **cumulative build** of the entire project, including all code from MVP 0 up to the current MVP-N.
- **Task:**
    1.  Resolve all dependency issues (`conan install`).
    2.  Configure the build system (`cmake`).
    3.  Compile all targets (`make`).
    4.  Run **all** unit tests from all completed MVPs.
- **Documentation:** The entire verification process, including any errors encountered and the steps taken to fix them, must be documented in the **"Build Verification"** section of the `DevHistory_MVP_N.md` file. A successful build and test run officially concludes the MVP.

---

## 4. General Principles

- **Embrace Iterative Debugging:** Expect the legacy code and build system to be broken. The workflow is designed to systematically find and fix these issues at each MVP stage.
- **Minimize Direct `replace` Usage:** For multi-line code changes, prefer reading the file, modifying the content in memory, and writing the entire file back (`read_file` -> `write_file`). Use `replace` only for small, targeted, single-line changes to avoid syntax errors.
- **Preserve History:** Once a `DevHistory_MVP_X.md` document is finalized and the MVP is complete, it should be considered immutable. Future changes to a feature are captured in a subsequent MVP, not by modifying past documents.
