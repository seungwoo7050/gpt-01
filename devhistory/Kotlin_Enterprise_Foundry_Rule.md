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

This phase establishes the workspace. The original legacy project is a **read-only reference** and must not be modified.

1.  **Create Project Root:** Create a new top-level directory named `Kotlin-Enterprise-Foundry-Reconstruction`.

2.  **Establish Directory Structure:** Inside the root, create the following subdirectories:
    ```
    Kotlin-Enterprise-Foundry-Reconstruction/
    ├── legacy_project/         # The original, untouched legacy codebase.
    ├── output/                 # The final, clean codebases for each of the 4 deliverables will be saved here.
    └── devhistory/             # All documentation, including this rulebook, will reside here.
    ```

3.  **Initialize Rules:** Place this document in the `devhistory/` directory as `Project_Rule.md`.

---

## 3. The Four Reconstruction Paths (The "What")

This project will produce four independent, fully documented, and tested application snapshots. The workflow will address each of these paths, using the others as context or a foundation.

1.  **Path 1: The Core Template (MVP 1-4)**
    - **Goal:** Reconstruct and document the creation of a generic, extensible web application template based on Hexagonal Architecture.
    - **Code Output:** `output/kotlin-template-mvp4/`
    - **Docs Output:** `devhistory/01_Core_Template/`

2.  **Path 2: The MSA Transformation (MVP 4.5)**
    - **Goal:** Document the refactoring of the Core Template into a Microservices Architecture, based on the code in the legacy project's root.
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
- **Action:** Using file search tools, locate all files and code snippets from the read-only legacy project that are relevant to the MVP's goal.
- **Task:** Compile a list of all legacy assets that will be reconstructed or referenced.

### 4.3. Implementation & Refactoring Strategy
- **Action:** Analyze the discovered assets and their dependencies.
- **Task:** Define a logical, step-by-step implementation plan. The plan must build from the ground up (e.g., Dependencies -> Domain Entities -> Repositories -> Services -> Controllers -> Integration).

### 4.4. Execution: Code Reconstruction & Verification
This is the primary implementation step, performed in the corresponding `output/` subdirectory.

- **The Golden Rule of Resequencing:** All legacy sequence comments (`// [SEQUENCE: 123]`) within the scope of the current MVP **must be removed**. They are replaced by a new, clean, and continuous sequence of comments (e.g., `// [SEQUENCE: MVP-N-1]`).

- **The Principle of Improvement:** Legacy code is **not to be trusted or copied blindly**. It must be improved:
    - **Refactor:** Improve code to be clean, readable, and maintainable, adhering to modern language idioms.
    - **Fix:** Correct all bugs, logical errors, and potential performance issues.
    - **Generate:** If code is missing, it must be created from scratch to meet the MVP's goal.

- **The Principle of Trust through Testing (TDD):**
    - Since the legacy code has no tests, its correctness is unproven. Trust is established via testing.
    - For every significant unit of logic (e.g., a service method, a complex domain entity), a corresponding **unit test must be written**.
    - The preferred workflow is to first write a **failing test** that precisely defines the requirements of the feature, then write the implementation code to make that test pass.

### 4.5. Documentation Regeneration
- **Action:** Once the code for an MVP is complete, tested, and re-sequenced, the corresponding `DevHistory_MVP_N.md` document must be written **from scratch**.
- **Task:** This document must be based **only on the new, verified code**. It must include an introduction (the "why"), a sequence list 100% synchronized with the code, and an "In-depth Analysis" section.

### 4.6. Cumulative Build & Verification
- **Action:** Each MVP is officially completed only after a successful cumulative build and test run.
- **Task:** For the current development path (e.g., Core Template), perform a full build of **all code from MVP 1 to the current MVP**. Run **all unit tests** from all completed MVPs in that path to guarantee no regressions have been introduced. This entire process, including any errors encountered and their resolutions, must be documented in the "Build Verification" section of the `DevHistory` document.

---

## 5. Universal Adaptation Rules

This SOP must be adapted to the realities of the legacy codebase.

- **If a `DEVELOPMENT_JOURNEY.md` is missing or incomplete:** The scope of an MVP must be inferred from file names, code comments, and logical dependencies. The proposed scope must be confirmed before proceeding.
- **If sequence comments are missing:** The primary task becomes one of architectural discovery. A logical sequence must be created from scratch based on an analysis of the code.
- **If code is missing:** The code must be generated from scratch based on the context of the surrounding application and the stated goal of the MVP.
