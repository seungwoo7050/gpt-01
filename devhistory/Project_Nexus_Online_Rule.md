# SOP for `Project_Nexus_Online` Reconstruction

## 1. Primary Objective & Guiding Philosophy

This document defines the Standard Operating Procedure (SOP) for reconstructing the `ping-pong` legacy project into a new, high-value portfolio asset named **`Project_Nexus_Online`**.

The **Primary Objective** is to methodically refactor, verify, and document the legacy codebase, transforming it from an unknown state into a reliable, production-grade, and well-documented distributed gaming platform. The project showcases an ambitious, polyglot microservices architecture featuring event sourcing and blockchain integration.

**Core Philosophy:** The fundamental goal of this process is to **build trust** in a complex legacy codebase through **verifiable, test-driven steps**. Trust is built by applying two key principles:
1.  **Test-Driven Development (TDD):** No legacy code is trusted. Its reliability must be proven by writing unit and integration tests.
2.  **Cumulative Builds per MVP:** Each development stage (MVP) is validated by a successful cumulative build and test run, ensuring the project remains stable at all times.

**Single Source of Truth:**
- **Code:** The newly written, verified source code in the `output/project-nexus-online/` directory is the only source of truth.
- **Specification:** The `DevHistory_MVP_X.md` document series is the official, human-readable specification for each incremental stage, 100% synchronized with the code.

---

## 2. Phase 0: Project Initialization & Workspace Setup

This phase establishes the workspace. The original legacy project must be treated as a **read-only reference** and must not be modified.

1.  **Create Project Root:** Create a new top-level directory named `Project_Nexus_Online_Reconstruction`.

2.  **Establish Directory Structure:** Inside the root, create the following subdirectories:
    ```
    Project_Nexus_Online_Reconstruction/
    ├── legacy_ping_pong/         # The original, untouched legacy codebase (read-only).
    ├── output/
    │   └── project-nexus-online/ # The single, clean, and evolving codebase will be built here.
    └── devhistory/
        └── Project_Rule.md       # This document.
    ```

---

## 3. Core Workflow: The Reconstruction Path

This project follows a single, linear development path that will be reconstructed and documented as a **series of per-MVP documents**.

- **Goal:** To analyze the monolithic legacy `DEVELOPMENT_JOURNEY.md` and break it down into its constituent MVPs (e.g., 1 through 8).
- **Documentation Output:** For each MVP, a corresponding `devhistory/DevHistory_MVP_X.md` file will be created.
- **Code Output:** All work will be performed on the single codebase inside `output/project-nexus-online/`, which will evolve with each completed MVP.

---

## 4. Standard Operating Procedure (The "How")

Each MVP in the series must strictly follow this iterative procedure.

### 4.1. Goal & Scope Definition
- **Action:** Analyze the relevant section in the legacy `DEVELOPMENT_JOURNEY.md` to understand the intended scope of the current MVP.
- **Task:** Define and confirm a single, clear, and concise goal for the MVP (e.g., "Implement the core tournament smart contract," "Establish the event sourcing pipeline").

### 4.2. Legacy Asset Analysis
- **Action:** Using file search tools, locate all files and code snippets from the read-only legacy project that are relevant to the MVP's goal.
- **Task:** Compile a list of all legacy assets that will be reconstructed or referenced.

### 4.3. Implementation & Refactoring Strategy
- **Action:** Analyze the discovered assets and their dependencies.
- **Task:** Define a logical, step-by-step implementation plan for the current MVP.

### 4.4. Execution: Code Reconstruction & Verification
This is the primary implementation step, performed in the `output/project-nexus-online/` directory.

- **The Golden Rule of Resequencing:** All legacy sequence comments (`// [SEQUENCE: 123]`) within the scope of the current MVP **must be removed**. They are replaced by a new, clean, and continuous sequence of comments (e.g., `// [SEQUENCE: MVP-N-1]`).

- **The Principle of Improvement:** Legacy code is **not to be copied blindly**. It must be improved:
    - **Refactor:** Improve code to be clean, readable, and maintainable.
    - **Fix:** Correct all bugs, logical errors, and performance issues.
    - **Generate:** If code is missing, it must be created from scratch to meet the MVP's goal.

- **The Principle of Trust through Testing (TDD):**
    - Since the legacy code has no tests, its correctness is unproven. Trust is established via testing.
    - For every significant unit of logic, a corresponding **unit test must be written**.
    - The preferred workflow is to first write a **failing test** that precisely defines the requirements, then write the implementation code to make that test pass.

### 4.5. Documentation Regeneration
- **Action:** Once the code for an MVP is complete, tested, and re-sequenced, the corresponding `devhistory/DevHistory_MVP_N.md` document must be written **from scratch**.
- **Task:** This document must be based **only on the new, verified code**. It must include an introduction (the "why"), a sequence list 100% synchronized with the code, and an "In-depth Analysis" section.

### 4.6. Cumulative Build & Verification
- **Action:** Each MVP is officially completed only after a successful cumulative build and test run.
- **Task:** Perform a full build of **all code from MVP 1 to the current MVP**. Run **all unit tests** from all completed MVPs to guarantee no regressions. This entire process, including any errors encountered and their resolutions, must be documented in the "Build Verification" section of the `DevHistory_MVP_N.md` file.

---

## 5. Universal Adaptation Rules

This SOP must be adapted to the realities of the legacy codebase.

- **If a `DEVELOPMENT_JOURNEY.md` is missing or incomplete:** The scope of an MVP must be inferred from file names, code comments, and logical dependencies. The proposed scope must be confirmed before proceeding.
- **If sequence comments are missing:** The primary task becomes one of architectural discovery. A logical sequence must be created from scratch based on an analysis of the code.
- **If code is missing:** The code must be generated from scratch based on the context of the surrounding application and the stated goal of the MVP.
