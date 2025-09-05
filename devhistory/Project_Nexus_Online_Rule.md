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
- **Task:** Define and confirm a single, clear, and concise goal for the MVP.

### 4.2. Legacy Asset Analysis
- **Action:** Using file search tools, locate all files from the read-only legacy project that are relevant to the MVP's goal.
- **Task:** Compile a list of all relevant legacy assets that will be reconstructed or referenced.

### 4.3. Implementation & Refactoring Strategy
- **Action:** Analyze the discovered assets and their dependencies.
- **Task:** Define a logical, step-by-step implementation plan.

### 4.4. Execution: Code Reconstruction & Verification
- **Sequence Annotation Principles:**
    - **Initial Annotation:** When encountering code with only legacy numeric comments (`// [SEQUENCE: 123]`), the old comments are **removed** and **replaced** with the new `// [SEQUENCE: MVP-N-X]` format.
    - **Historical Annotation:** When encountering code that **already has** a comment from a previous MVP (e.g., `// [SEQUENCE: MVP-A-B]`), the old comment **must be preserved**. The new `// [SEQUENCE: MVP-N-X]` comment for the current work should be **added directly below it**.

- **Code Modification Principles:**
    - **Improve, Don't Just Copy:** Refactor legacy code for clarity, correctness, and to apply modern design patterns.
    - **Preserve, Don't Delete:** When a feature is deprecated, the old code should be **commented out** with an explanatory note rather than being deleted.

- **The Principle of Trust through Testing (TDD):**
    - For every significant unit of logic, a corresponding **unit test must be written**.
    - The preferred workflow is to first write a **failing test** that defines the requirements, then write the implementation code to make that test pass.

### 4.5. `DevHistory` Document Regeneration
- **Action:** After coding is complete, create the `devhistory/DevHistory_MVP_N.md` document **from scratch**.
- **Task:** This document must be based **only on the new, verified code**. It must contain three key sections in order:
    1.  **Introduction:** Explaining the "why" of the MVP.
    2.  **In-depth Analysis for Technical Interviews:** Explaining architectural choices, trade-offs, and **Production Considerations**.
    3.  **Sequence List:** The complete list of all `[SEQUENCE: MVP-N-X]` markers.

### 4.6. Cumulative Build & Verification
- **Action:** Each MVP is officially completed only after a successful cumulative build and test run.
- **Task:** Perform a full, **cumulative build** of the project up to the current MVP. Run **all unit tests** from all completed MVPs to guarantee no regressions. Document this process, including errors and fixes, in a final **"Build Verification"** section of the `DevHistory` document.

---

## 5. General Principles & Procedures

### 5.1. Build Error Resolution Procedure
1.  **Analyze:** Read the compiler/linker error message carefully.
2.  **Isolate:** Try to reproduce the error in a minimal example.
3.  **Check Dependencies:** Review build files (`build.gradle.kts`, `CMakeLists.txt`, etc.) to ensure libraries are correctly configured.
4.  **Solve Incrementally:** Fix one error at a time and attempt to rebuild.

### 5.2. Adaptation to Legacy State
- **If a `DEVELOPMENT_JOURNEY.md` is missing:** Infer the MVP scope from file names and code comments.
- **If sequence comments are missing:** Create a logical sequence from scratch based on code analysis.
- **If code is missing:** Generate the code from scratch based on the MVP goal.