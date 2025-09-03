#!/bin/bash

# [SEQUENCE: MVP6-7] An automation script for building and deploying the server.
# [SEQUENCE: 1] Deployment script for MMORPG game server
set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# [SEQUENCE: 2] Configuration
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DEPLOYMENT_ENV="${DEPLOYMENT_ENV:-development}"
DOCKER_REGISTRY="${DOCKER_REGISTRY:-}"
IMAGE_TAG="${IMAGE_TAG:-latest}"

# [SEQUENCE: 3] Helper functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# [SEQUENCE: 4] Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check Docker
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed"
    fi
    
    # Check Docker Compose
    if ! command -v docker-compose &> /dev/null; then
        log_error "Docker Compose is not installed"
    fi
    
    # Check kubectl for K8s deployment
    if [[ "$DEPLOYMENT_ENV" == "production" ]]; then
        if ! command -v kubectl &> /dev/null; then
            log_error "kubectl is not installed"
        fi
    fi
    
    log_success "All prerequisites met"
}

# [SEQUENCE: 5] Build Docker image
build_image() {
    log_info "Building Docker image..."
    
    cd "$PROJECT_ROOT"
    
    # Build with BuildKit for better caching
    DOCKER_BUILDKIT=1 docker build \
        --target runtime \
        --tag mmorpg/game-server:${IMAGE_TAG} \
        --tag mmorpg/game-server:latest \
        --build-arg BUILD_DATE=$(date -u +'%Y-%m-%dT%H:%M:%SZ') \
        --build-arg VCS_REF=$(git rev-parse --short HEAD) \
        .
    
    log_success "Docker image built successfully"
}

# [SEQUENCE: 6] Run tests
run_tests() {
    log_info "Running tests..."
    
    # Build test image
    DOCKER_BUILDKIT=1 docker build \
        --target builder \
        --tag mmorpg/game-server:test \
        .
    
    # Run tests in container
    docker run --rm \
        mmorpg/game-server:test \
        bash -c "cd /app/build && ctest --output-on-failure"
    
    log_success "All tests passed"
}

# [SEQUENCE: 7] Push to registry
push_image() {
    if [[ -z "$DOCKER_REGISTRY" ]]; then
        log_warning "No Docker registry specified, skipping push"
        return
    fi
    
    log_info "Pushing image to registry..."
    
    docker tag mmorpg/game-server:${IMAGE_TAG} \
        ${DOCKER_REGISTRY}/mmorpg/game-server:${IMAGE_TAG}
    
    docker push ${DOCKER_REGISTRY}/mmorpg/game-server:${IMAGE_TAG}
    
    log_success "Image pushed to registry"
}

# [SEQUENCE: 8] Deploy with Docker Compose
deploy_compose() {
    log_info "Deploying with Docker Compose..."
    
    cd "$PROJECT_ROOT"
    
    # Create .env file if it doesn't exist
    if [[ ! -f .env ]]; then
        cp .env.example .env
        log_warning "Created .env file from .env.example - please review settings"
    fi
    
    # Pull latest images
    docker-compose pull
    
    # Deploy with zero downtime
    docker-compose up -d --scale game-server-zone1=2 --scale game-server-zone2=2
    
    # Wait for health checks
    sleep 10
    
    # Check deployment status
    docker-compose ps
    
    log_success "Docker Compose deployment complete"
}

# [SEQUENCE: 9] Deploy to Kubernetes
deploy_kubernetes() {
    log_info "Deploying to Kubernetes..."
    
    cd "$PROJECT_ROOT/k8s"
    
    # Apply base configuration
    kubectl apply -k base/
    
    # Apply environment-specific overlays
    kubectl apply -k overlays/${DEPLOYMENT_ENV}/
    
    # Wait for rollout
    kubectl -n mmorpg-game rollout status deployment/game-server
    
    # Show deployment status
    kubectl -n mmorpg-game get pods
    
    log_success "Kubernetes deployment complete"
}

# [SEQUENCE: 10] Run database migrations
run_migrations() {
    log_info "Running database migrations..."
    
    # For Docker Compose
    if [[ "$DEPLOYMENT_ENV" == "development" ]]; then
        docker-compose exec postgres psql -U gameserver -d gamedb \
            -f /docker-entrypoint-initdb.d/01-init.sql
    else
        # For K8s, use a Job
        kubectl apply -f k8s/jobs/db-migration.yaml
        kubectl wait --for=condition=complete job/db-migration -n mmorpg-game
    fi
    
    log_success "Database migrations complete"
}

# [SEQUENCE: 11] Health check
health_check() {
    log_info "Performing health checks..."
    
    if [[ "$DEPLOYMENT_ENV" == "development" ]]; then
        HEALTH_URL="http://localhost:8081/health"
    else
        HEALTH_URL=$(kubectl -n mmorpg-game get svc game-server-lb -o jsonpath='{.status.loadBalancer.ingress[0].hostname}'):8081/health
    fi
    
    # Wait for service to be ready
    for i in {1..30}; do
        if curl -sf "$HEALTH_URL" > /dev/null; then
            log_success "Health check passed"
            return
        fi
        sleep 2
    done
    
    log_error "Health check failed after 60 seconds"
}

# [SEQUENCE: 12] Main deployment flow
main() {
    log_info "Starting deployment for environment: $DEPLOYMENT_ENV"
    
    check_prerequisites
    
    if [[ "${SKIP_BUILD:-}" != "true" ]]; then
        build_image
        run_tests
        push_image
    fi
    
    case "$DEPLOYMENT_ENV" in
        development)
            deploy_compose
            ;;
        staging|production)
            deploy_kubernetes
            ;;
        *)
            log_error "Unknown environment: $DEPLOYMENT_ENV"
            ;;
    esac
    
    if [[ "${SKIP_MIGRATIONS:-}" != "true" ]]; then
        run_migrations
    fi
    
    health_check
    
    log_success "Deployment completed successfully!"
    
    # Show access information
    echo ""
    echo "Access Information:"
    echo "==================="
    if [[ "$DEPLOYMENT_ENV" == "development" ]]; then
        echo "Game Server: http://localhost:8080"
        echo "Grafana: http://localhost:3000 (admin/admin123)"
        echo "RabbitMQ: http://localhost:15672 (game/game123)"
        echo "Prometheus: http://localhost:9000"
    else
        echo "Game Server: Use kubectl to get LoadBalancer IP"
        echo "kubectl -n mmorpg-game get svc game-server-lb"
    fi
}

# [SEQUENCE: 13] Handle command line arguments
case "${1:-deploy}" in
    deploy)
        main
        ;;
    build)
        check_prerequisites
        build_image
        ;;
    test)
        check_prerequisites
        run_tests
        ;;
    migrate)
        run_migrations
        ;;
    *)
        echo "Usage: $0 {deploy|build|test|migrate}"
        exit 1
        ;;
esac