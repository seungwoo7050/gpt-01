# Getting Started

This document provides step-by-step instructions to set up the development environment, build the project, and run the server and related tools.

---

## 1. Prerequisites

Before you begin, ensure you have the following tools installed on your system (Ubuntu is the primary supported OS):

- **C++ Compiler:** A modern C++ compiler supporting C++20 (e.g., GCC 11 or later).
- **CMake:** Version 3.20 or later.
- **Conan:** The C/C++ Package Manager, version 2.0 or later.
- **Git:** For cloning the repository.
- **OpenSSL:** For generating self-signed certificates for local testing.
- **Docker & Docker Compose (Optional):** For running the full local development stack, including database and cache services.

---

## 2. Building the Project

The project uses a standard Conan and CMake workflow. All commands should be run from the project's root directory (`gpt-01/`).

### Step 1: Install Dependencies

First, use Conan to install all the C++ dependencies defined in `conanfile.txt`.

```bash
cd ecs-realm-server
conan install . --build=missing
cd ..
```
This command will download and set up libraries like Boost, spdlog, Redis, MySQL, etc., and generate the `conan_toolchain.cmake` file required by CMake.

### Step 2: Configure CMake

Next, run CMake to generate the build files. We will create a `build` directory to keep the project clean.

```bash
cd ecs-realm-server/build
cmake ..
```
**Note:** The `CMakeLists.txt` is configured to automatically include the `conan_toolchain.cmake` file, so you do not need to pass it as a command-line argument.

### Step 3: Compile

Finally, compile the project using `make`. You can build all targets or a specific target.

```bash
# To build everything (server, tests, load client)
make -j

# To build only the server
make -j mmorpg_server

# To build only the unit tests
make -j unit_tests

# To build only the load test client
make -j load_test_client
```

---

## 3. Running the Server

### Step 1: Generate SSL Certificate

The server requires an SSL certificate and private key to run. For local development, you can generate a self-signed certificate. Run this command from the project root directory.

```bash
openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes -subj "/C=US/ST=California/L=San-Francisco/O=My-Game-Company/OU=Dev/CN=localhost"
```

### Step 2: Run the Server Executable

With the certificate generated, you can now run the server. The executable will be located in the `ecs-realm-server/build/` directory.

```bash
# Run in the foreground
./ecs-realm-server/build/mmorpg_server

# Run in the background and log output to a file
./ecs-realm-server/build/mmorpg_server > server.log 2>&1 &
```
**Note:** The server requires a running MySQL and Redis instance. For local development, it is highly recommended to use the provided Docker Compose setup (see Deployment Guide).

---

## 4. Running Tests

### Unit Tests

After building the `unit_tests` target, you can run it to verify the correctness of the core components.

```bash
./ecs-realm-server/build/unit_tests
```

### Load Test

After building the `load_test_client` target, you can run it to simulate multiple clients connecting to the server.

```bash
# Run with default settings (100 clients for 30s)
./ecs-realm-server/build/load_test_client

# Run with custom settings
./ecs-realm-server/build/load_test_client --clients 50 --duration 10 --pps 20
```
