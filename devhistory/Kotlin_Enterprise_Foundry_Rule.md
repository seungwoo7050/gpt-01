# SOP for `Kotlin-Enterprise-Foundry` Reconstruction

## 1. Primary Objective & Guiding Philosophy

This document defines the Standard Operating Procedure (SOP) for reconstructing the `korean-enterprise-template-claude` legacy project into a new, high-value portfolio asset named **`Kotlin-Enterprise-Foundry`**.

The **Primary Objective** is to methodically transform an undocumented and unreliable legacy codebase into a suite of four distinct, production-quality applications. This is achieved by building a verifiable, step-by-step development history for each application path.

**Core Philosophy:** The fundamental goal of this process is to **build trust and create value** from an unknown codebase. Trust is built by applying two key principles:
1.  **Test-Driven Development (TDD):** No legacy code is trusted. Its reliability is proven by writing unit tests.
2.  **Cumulative Builds:** Each development stage (MVP) is validated by a successful cumulative build and test run, ensuring the project remains stable at all times.

**Single Source of Truth:**
- **Code:** The newly written, verified source code in the `output/` subdirectories is the only source of truth.
- **Specification:** The `DevHistory` markdown files are the official, human-readable specifications for each development stage, 100% synchronized with the code.

---

## 2. Phase 0: Project Initialization & Workspace Setup

This phase establishes the workspace. The original legacy project must be treated as a **read-only reference** and should not be modified.

1.  **Create Project Root:** Create a new top-level directory named `Kotlin-Enterprise-Foundry-Reconstruction`.

2.  **Establish Directory Structure:** Inside the root, create the following subdirectories:
    ```
    Kotlin-Enterprise-Foundry-Reconstruction/
    ├── legacy_project/         # The original, untouched legacy codebase (read-only).
    ├── output/                 # The 4 final, clean codebases will be saved here as snapshots.
    └── devhistory/             # All documentation, including this rulebook, will reside here.
    ```

3.  **Initialize Rules:** Place this document inside the `devhistory/` directory as `Project_Rule.md`.

---

## 3. The Four Reconstruction Paths (The "What")

This project will produce four independent, fully documented, and tested application snapshots. The workflow will address each of these paths.

1.  **Path 1: The Core Template (MVP 1-4)**
    - **Goal:** Reconstruct and document the creation of a generic, extensible web application template based on Hexagonal Architecture.
    - **Code Output:** `output/kotlin-template-mvp4/`
    - **Docs Output:** `devhistory/01_Core_Template/`

2.  **Path 2: The MSA Transformation (MVP 4.5)**
    - **Goal:** Document the refactoring of the Core Template into a Microservices Architecture.
    - **Code Output:** `output/kotlin-msa-mvp4.5/`
    - **Docs Output:** `devhistory/02_MSA_Transformation/`

3.  **Path 3: The E-commerce Application (MVP 5)**
    - **Goal:** Document the extension of the Core Template into a specific E-commerce web application.
    - **Code Output:** `output/kotlin-ecommerce-mvp5/`
    - **Docs Output:** `devhistory/03_WebApp_Ecommerce/`

4.  **Path 4: The Fintech Application (MVP 5)**
    - **Goal:** Document the extension of the Core Template into a specific Fintech web application.
    - **Code Output:** `output/kotlin-fintech-mvp5/`
    - **Docs Output:** `devhistory/04_WebApp_Fintech/`

---

## 4. Standard Operating Procedure (The "How")

Each MVP within each of the four paths must strictly follow this iterative procedure.

### 4.1. Goal & Scope Definition
- **Action:** Analyze the relevant legacy `DEVELOPMENT_JOURNEY.md` to understand the intended scope of the current MVP.
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
- **Action:** After coding is complete, create the `devhistory/path/DevHistory_MVP_N.md` document **from scratch**.
- **Task:** This document must be based **only on the new, verified code**. It must contain three key sections in order:
    1.  **Introduction:** Explaining the "why" of the MVP.
    2.  **In-depth Analysis for Technical Interviews:** Explaining architectural choices, trade-offs, and **Production Considerations**.
    3.  **Sequence List:** The complete list of all `[SEQUENCE: MVP-N-X]` markers.

### 4.6. Cumulative Build & Verification
- **Action:** Each MVP is officially completed only after a successful cumulative build and test run.
- **Task:** Perform a full, **cumulative build** of the current path. Run **all unit tests** from all completed MVPs in that path. Document this process, including errors and fixes, in a final **"Build Verification"** section of the `DevHistory` document.

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