#!/bin/bash
# [SEQUENCE: MVP6-7] Deployment script for MMORPG game server
set -euo pipefail

# ... (helper functions and configuration) ...

# [SEQUENCE: MVP6-25] Build Docker image
build_image() { 
    echo "Building Docker image..."
    DOCKER_BUILDKIT=1 docker build -t mmorpg/game-server:${IMAGE_TAG:-latest} .
}

# [SEQUENCE: MVP6-26] Run tests
run_tests() { 
    echo "Running tests..."
    docker run --rm mmorpg/game-server:${IMAGE_TAG:-latest} ctest
}

# [SEQUENCE: MVP6-27] Deploy with Docker Compose
deploy_compose() { 
    echo "Deploying with Docker Compose..."
    docker-compose up -d
}

# [SEQUENCE: MVP6-28] Deploy to Kubernetes
deploy_kubernetes() { 
    echo "Deploying to Kubernetes in env $DEPLOYMENT_ENV..."
    kubectl apply -k k8s/overlays/${DEPLOYMENT_ENV}
}

# [SEQUENCE: MVP6-29] Main deployment flow
main() {
    check_prerequisites
    build_image
    run_tests
    
    case "$DEPLOYMENT_ENV" in
        development) deploy_compose ;; 
        staging|production) deploy_kubernetes ;; 
    esac
    
    health_check
}

main "$@"
