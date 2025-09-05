# Deployment Guide

This document explains how to deploy the server using containerization technologies like Docker and Kubernetes. This is the recommended approach for both local development and production environments.

---

## 1. Docker

The project includes a multi-stage `Dockerfile` to create an optimized and secure container image for the server.

### Building the Image

You can build the Docker image from the project root directory:

```bash
docker build -t mmorpg-server:latest -f ecs-realm-server/Dockerfile .
```

### Multi-Stage Build Explained

- **`builder` Stage:** This stage installs all necessary build-time dependencies (CMake, Conan, C++ compiler, etc.) and compiles the C++ application in release mode. 
- **`runtime` Stage:** This is the final, lightweight stage. It starts from a clean Ubuntu base image and copies *only* the compiled server executable from the `builder` stage. This has two major benefits:
    1.  **Reduced Size:** The final image is significantly smaller as it does not contain any build tools or source code.
    2.  **Increased Security:** The attack surface is minimized by excluding unnecessary tools and libraries.

### Security

The runtime image is configured to run as a non-root user (`gameserver`) for enhanced security.

---

## 2. Docker Compose (Local Development)

For a comprehensive local development and testing environment, a `docker-compose.yml` file is provided in the `ecs-realm-server/` directory.

### Services

This file defines all the services required to run the full application stack:

- **`game-server-zone1`:** The main C++ MMORPG server.
- **`postgres`:** The PostgreSQL database for data persistence.
- **`redis`:** The Redis server for caching and distributed locking.
- **`rabbitmq`:** A message queue for potential inter-server communication (placeholder).
- **Monitoring Stack:**
    - **`prometheus`:** For collecting time-series metrics from the server.
    - **`grafana`:** For visualizing the metrics collected by Prometheus.

### Usage

To start the entire local stack, run the following command from the `ecs-realm-server/` directory:

```bash
docker-compose up -d
```

To stop and remove all containers:

```bash
docker-compose down
```

---

## 3. Kubernetes (Production Deployment)

The `k8s/` directory contains manifest files for deploying the server to a Kubernetes cluster. This represents a production-ready deployment strategy.

### Manifests

- **`k8s/configmap.yaml`:** Externalizes configuration details (like log level and database hosts) from the container image. This allows you to change configuration without rebuilding the Docker image.

- **`k8s/deployment.yaml`:** Defines the desired state for the running application. Key features include:
    - **`replicas`:** Specifies how many instances (pods) of the server should be running.
    - **`envFrom`:** Injects configuration from the `ConfigMap`.
    - **`readinessProbe` & `livenessProbe`:** Health checks that allow Kubernetes to automatically manage the lifecycle of the server pods, restarting them if they become unresponsive.

- **`k8s/service.yaml`:** Exposes the running server pods to the internet. It defines a `LoadBalancer`, which will provision an external load balancer in a cloud environment (like AWS, GCP, Azure) to distribute traffic to the server pods.

### Deployment Steps

Assuming you have a configured Kubernetes cluster (e.g., Minikube for local testing, or a cloud provider):

1.  **Build and Push the Docker Image:** Build the image as described above and push it to a container registry (like Docker Hub, GCR, or ECR).
    ```bash
    docker tag mmorpg-server:latest your-registry/mmorpg-server:latest
    docker push your-registry/mmorpg-server:latest
    ```

2.  **Update Image Path:** In `k8s/deployment.yaml`, change the `image:` field to point to your container registry.

3.  **Apply the Manifests:** Apply all the configuration files to your cluster.
    ```bash
    kubectl apply -f ecs-realm-server/k8s/configmap.yaml
    kubectl apply -f ecs-realm-server/k8s/deployment.yaml
    kubectl apply -f ecs-realm-server/k8s/service.yaml
    ```
