# Extension 00: CI/CD Pipeline Enhancement

## 1. Objective
This guide details how to enhance the existing CI/CD pipeline to automatically publish the server's Docker image to a container registry and add a build status badge to the project's `README.md`. This provides immediate feedback on the project's health and automates the distribution of server builds.

## 2. Analysis of Current Implementation

### 2.1. `README.md`
The project currently lacks a `README.md` file. This is the front page of the project and should contain essential information, including a status badge indicating the result of the latest build.

### 2.2. `.github/workflows/ci.yml`
The current CI workflow, located at `.github/workflows/ci.yml`, successfully builds and tests the project. It even builds a Docker image. However, it has a significant limitation.

**Legacy Code (`.github/workflows/ci.yml`):**
```yaml
# [SEQUENCE: MVP8-14] A GitHub Actions CI pipeline to automatically build and test the server on every push and pull request.
# This ensures code quality and stability by catching errors early.
name: C++ CI

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  build-and-test:
    runs-on: ubuntu-latest

    steps:
    - name: Checkout repository
      uses: actions/checkout@v3
      with:
        submodules: 'recursive'

    - name: Set up Conan
      uses: conan-io/cmake-conan-action@v1

    - name: Install Dependencies
      run: conan install ecs-realm-server/ --output-folder=ecs-realm-server/build --build=missing

    - name: Configure CMake
      run: cmake -S ecs-realm-server -B ecs-realm-server/build

    - name: Build Project
      run: make -C ecs-realm-server/build -j$(nproc)

    - name: Run Unit Tests
      run: ecs-realm-server/build/unit_tests

    - name: Build Docker Image
      run: docker build . -f ecs-realm-server/Dockerfile -t mmorpg-server:latest
```
The final step, `Build Docker Image`, creates the image `mmorpg-server:latest` only on the local runner. It is not pushed to a shared registry like Docker Hub or GitHub Container Registry (GHCR), making it unavailable for deployment or download.

## 3. Proposed Implementation

### 3.1. Create `README.md` and Add Status Badge

First, create a new file named `README.md` at the project root (`/home/woopinbells/Desktop/gpt-01/README.md`).

**New File (`README.md`):**
```markdown
# C++ Multiplayer Game Server (ecs-realm-server)

[![C++ CI](https://github.com/YOUR_USERNAME/YOUR_REPOSITORY/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/YOUR_REPOSITORY/actions/workflows/ci.yml)

A high-performance, scalable C++ multiplayer game server built with a modern ECS architecture.

## Features
- TCP/UDP networking with SSL
- Entity Component System (ECS)
- PvP and Guild Systems
- And more...
```
**Action:** You must replace `YOUR_USERNAME/YOUR_REPOSITORY` with your actual GitHub username and repository name for the badge to work.

### 3.2. Enhance CI Workflow for Docker Push

Modify the `.github/workflows/ci.yml` file to log in to a container registry and push the image upon a successful build on the `main` branch. This example uses GitHub Container Registry (GHCR) for its tight integration with GitHub Actions.

**Modified File (`.github/workflows/ci.yml`):**
```yaml
# [SEQUENCE: MVP8-14] A GitHub Actions CI pipeline to automatically build and test the server on every push and pull request.
# This ensures code quality and stability by catching errors early.
name: C++ CI

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  build-and-test:
    runs-on: ubuntu-latest
    # [MODIFIED] Add permissions for writing to GHCR
    permissions:
      contents: read
      packages: write

    steps:
    - name: Checkout repository
      uses: actions/checkout@v3
      with:
        submodules: 'recursive'

    - name: Set up Conan
      uses: conan-io/cmake-conan-action@v1

    - name: Install Dependencies
      run: conan install ecs-realm-server/ --output-folder=ecs-realm-server/build --build=missing

    - name: Configure CMake
      run: cmake -S ecs-realm-server -B ecs-realm-server/build

    - name: Build Project
      run: make -C ecs-realm-server/build -j$(nproc)

    - name: Run Unit Tests
      run: ecs-realm-server/build/unit_tests

    # [NEW] Login to GitHub Container Registry
    - name: Log in to GHCR
      if: github.event_name != 'pull_request'
      uses: docker/login-action@v2
      with:
        registry: ghcr.io
        username: ${{ github.repository_owner }}
        password: ${{ secrets.GITHUB_TOKEN }}

    # [MODIFIED] Build and push Docker image
    - name: Build and push Docker image
      uses: docker/build-push-action@v4
      with:
        context: .
        file: ./ecs-realm-server/Dockerfile
        push: ${{ github.event_name != 'pull_request' }}
        tags: ghcr.io/${{ github.repository }}:latest
```

## 4. Rationale for Changes

*   **Permissions:** The `permissions` block is added to grant the workflow the necessary token scope to push packages (Docker images) to GHCR.
*   **Login Step:** The `docker/login-action` step securely authenticates against GHCR using a temporary `GITHUB_TOKEN`. This is safer than using personal access tokens.
*   **Build and Push Step:** The original `docker build` command is replaced with the more powerful `docker/build-push-action`.
    *   `push: ${{ github.event_name != 'pull_request' }}`: This crucial condition ensures that the image is only pushed when changes are merged into the `main` branch, not for every pull request build.
    *   `tags: ghcr.io/${{ github.repository }}:latest`: This tags the image correctly for GHCR, associating it with your repository (e.g., `ghcr.io/your-username/your-repo:latest`).

## 5. Testing Guide

1.  **Implement the Changes:** Create the `README.md` file and update the `.github/workflows/ci.yml` file as described above.
2.  **Commit and Push:** Commit these changes to a feature branch and open a pull request to `main`.
3.  **Verify PR Build:** Go to the "Actions" tab on your GitHub repository. You should see a new workflow run. Verify that all steps pass, but note that the "Log in to GHCR" and "Build and push" steps will be skipped because of the `if: github.event_name != 'pull_request'` condition.
4.  **Merge to Main:** Merge the pull request into the `main` branch.
5.  **Verify Main Build and Push:** A new workflow will trigger for the push to `main`. This time, verify that the "Log in to GHCR" and "Build and push" steps execute successfully.
6.  **Check Container Registry:** Go to your repository's main page, and in the right-hand sidebar, click on "Packages". You should see the newly pushed `mmorpg-server` image.
7.  **Check README Badge:** Navigate to your repository's main page. The `README.md` should now display a "passing" status badge.
