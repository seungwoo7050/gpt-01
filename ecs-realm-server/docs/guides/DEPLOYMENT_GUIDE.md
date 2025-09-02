# MMORPG Game Server Deployment Guide

## Overview

This guide covers deployment procedures for the C++ MMORPG game server across different environments: development, staging, and production.

## Prerequisites

### Required Tools
- Docker 20.10+
- Docker Compose 2.0+
- kubectl 1.24+ (for Kubernetes deployment)
- Git
- Make/CMake 3.20+

### System Requirements
- **Development**: 4 CPU cores, 8GB RAM
- **Production**: 8+ CPU cores, 16GB+ RAM per server

## Quick Start

### Local Development
```bash
# Clone repository
git clone https://github.com/your-org/cpp-multiplayer-game-server.git
cd cpp-multiplayer-game-server

# Copy environment configuration
cp .env.example .env
# Edit .env with your settings

# Build and start services
docker-compose up -d

# Check status
docker-compose ps

# View logs
docker-compose logs -f game-server-zone1
```

### Production Deployment
```bash
# Set environment
export DEPLOYMENT_ENV=production
export DOCKER_REGISTRY=your-registry.com

# Deploy
./scripts/deploy.sh
```

## Architecture

### Service Components

1. **Game Servers**: Handle player connections and game logic
   - Zone-based distribution
   - Horizontal scaling
   - Stateless design where possible

2. **Load Balancer**: Nginx for connection distribution
   - TCP streaming for game traffic
   - Health check endpoints
   - SSL/TLS termination

3. **Data Layer**:
   - PostgreSQL: Persistent player data
   - Redis: Session state, matchmaking queues
   - RabbitMQ: Cross-server messaging

4. **Monitoring**:
   - Prometheus: Metrics collection
   - Grafana: Visualization
   - Jaeger: Distributed tracing

## Deployment Environments

### Development (Docker Compose)

Directory structure:
```
docker-compose.yml     # Main configuration
.env                  # Environment variables
logs/                 # Log output
config/               # Server configuration
```

Commands:
```bash
# Start all services
docker-compose up -d

# Scale game servers
docker-compose up -d --scale game-server-zone1=3

# Stop services
docker-compose down

# Reset everything (including volumes)
docker-compose down -v
```

### Production (Kubernetes)

Directory structure:
```
k8s/
├── base/            # Common resources
├── overlays/
│   ├── dev/        # Development overrides
│   └── prod/       # Production overrides
```

Commands:
```bash
# Deploy to production
kubectl apply -k k8s/overlays/prod/

# Check deployment
kubectl -n mmorpg-prod get pods

# Scale deployment
kubectl -n mmorpg-prod scale deployment/game-server --replicas=10

# Rolling update
kubectl -n mmorpg-prod set image deployment/game-server \
  game-server=your-registry.com/mmorpg/game-server:v1.1.0
```

## Configuration

### Environment Variables

Key configuration options:

```bash
# Server Configuration
WORLD_ZONE_ID=1              # Zone this server handles
MAX_PLAYERS=1000             # Maximum concurrent players
TICK_RATE=30                 # Game loop frequency (Hz)

# Database
DB_HOST=postgres
DB_NAME=gamedb
DB_USER=gameserver
DB_PASSWORD=secure_password

# Redis
REDIS_URL=redis://redis:6379/0
REDIS_PASSWORD=redis_password

# Monitoring
LOG_LEVEL=info               # debug|info|warn|error
ENABLE_METRICS=true
```

### Server Configuration (server.yaml)

```yaml
server:
  name: "Zone 1"
  port: 8080
  
network:
  max_connections: 10000
  timeout: 300
  
world:
  tick_rate: 30
  save_interval: 300
  
combat:
  system: "action"    # "targeted" or "action"
  
pvp:
  arena_instances: 100
  matchmaking_interval: 5
```

## Monitoring

### Access Points

- **Grafana**: http://localhost:3000 (admin/admin123)
- **Prometheus**: http://localhost:9000
- **Jaeger**: http://localhost:16686
- **RabbitMQ Management**: http://localhost:15672 (game/game123)

### Key Metrics

1. **Server Health**:
   - `game_server_players_online`: Current player count
   - `game_server_tick_duration_seconds`: Game loop performance
   - `process_cpu_seconds_total`: CPU usage
   - `process_resident_memory_bytes`: Memory usage

2. **Network**:
   - `game_server_packets_received_total`: Incoming traffic
   - `game_server_packets_sent_total`: Outgoing traffic
   - `game_server_network_latency_seconds`: Connection latency

3. **Game Systems**:
   - `game_server_combat_events_total`: Combat activity
   - `game_server_matchmaking_queue_size`: PvP queue length
   - `game_server_zone_players`: Players per zone

### Alerts

Pre-configured alerts:
- Server down
- High CPU/Memory usage
- Database connection issues
- Long matchmaking queues
- High tick latency

## Operations

### Health Checks

Endpoints:
- `/health`: Basic liveness check
- `/ready`: Readiness check (includes dependencies)
- `/metrics`: Prometheus metrics

Example:
```bash
curl http://localhost:8081/health
# {"status":"healthy","uptime":3600}
```

### Backup Procedures

1. **Database Backup**:
```bash
# Manual backup
docker-compose exec postgres pg_dump -U gameserver gamedb > backup.sql

# Scheduled backup (add to crontab)
0 2 * * * docker-compose exec -T postgres pg_dump -U gameserver gamedb > /backups/gamedb-$(date +\%Y\%m\%d).sql
```

2. **Redis Snapshot**:
```bash
docker-compose exec redis redis-cli BGSAVE
```

### Scaling

1. **Horizontal Scaling**:
   - Add more game server instances
   - Distribute zones across servers
   - Use Redis for shared state

2. **Vertical Scaling**:
   - Increase container resources
   - Optimize tick rate for player count
   - Tune database connections

### Troubleshooting

Common issues:

1. **High Memory Usage**:
   - Check player count
   - Review entity lifecycle
   - Monitor spatial index size

2. **Network Lag**:
   - Check bandwidth usage
   - Review update frequencies
   - Monitor packet sizes

3. **Database Bottlenecks**:
   - Check connection pool
   - Review slow queries
   - Add read replicas

## Security

### Best Practices

1. **Network Security**:
   - Use TLS for all external connections
   - Implement rate limiting
   - Validate all input

2. **Container Security**:
   - Run as non-root user
   - Use minimal base images
   - Regular security updates

3. **Secrets Management**:
   - Use Kubernetes secrets
   - Rotate credentials regularly
   - Never commit secrets to git

### SSL/TLS Configuration

For production:
```nginx
server {
    listen 443 ssl;
    ssl_certificate /etc/nginx/ssl/game.crt;
    ssl_certificate_key /etc/nginx/ssl/game.key;
    ssl_protocols TLSv1.2 TLSv1.3;
}
```

## Maintenance

### Rolling Updates

Zero-downtime deployment:
```bash
# Docker Compose
docker-compose up -d --no-deps --build game-server-zone1

# Kubernetes
kubectl rollout restart deployment/game-server -n mmorpg-prod
kubectl rollout status deployment/game-server -n mmorpg-prod
```

### Log Management

Log locations:
- Game server: `/app/logs/game.log`
- Nginx: `/var/log/nginx/access.log`
- PostgreSQL: `/var/log/postgresql/postgresql.log`

Log rotation:
```bash
# logrotate configuration
/app/logs/*.log {
    daily
    rotate 7
    compress
    delaycompress
    missingok
    notifempty
}
```

## Disaster Recovery

### Backup Strategy

1. **Daily Backups**:
   - Database: Full backup
   - Redis: RDB snapshot
   - Configuration: Git repository

2. **Recovery Time Objective (RTO)**: < 1 hour
3. **Recovery Point Objective (RPO)**: < 24 hours

### Restore Procedures

```bash
# Restore database
psql -U gameserver -d gamedb < backup.sql

# Restore Redis
docker-compose stop redis
docker cp backup.rdb mmorpg-redis:/data/dump.rdb
docker-compose start redis
```

## Performance Tuning

### Linux Kernel Parameters

```bash
# /etc/sysctl.conf
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.ip_local_port_range = 1024 65535
net.core.netdev_max_backlog = 65535
```

### Database Tuning

```sql
-- PostgreSQL configuration
shared_buffers = 4GB
effective_cache_size = 12GB
max_connections = 200
work_mem = 10MB
```

## Support

### Monitoring Checklist

Daily:
- [ ] Check error logs
- [ ] Review player counts
- [ ] Monitor resource usage

Weekly:
- [ ] Review performance metrics
- [ ] Check backup integrity
- [ ] Update dependencies

### Contact

- Issues: https://github.com/your-org/cpp-multiplayer-game-server/issues
- Slack: #game-server-ops
- On-call: Use PagerDuty

## Appendix

### Useful Commands

```bash
# Get pod logs
kubectl logs -f deployment/game-server -n mmorpg-prod

# Execute command in container
docker-compose exec game-server-zone1 /bin/bash

# Database console
docker-compose exec postgres psql -U gameserver -d gamedb

# Redis CLI
docker-compose exec redis redis-cli

# Port forwarding (K8s)
kubectl port-forward svc/game-server-service 8080:8080 -n mmorpg-prod
```

### Environment Migration

Moving from dev to prod:
1. Export data from development
2. Update configuration for production
3. Import data to production
4. Verify all services
5. Update DNS/Load balancer
6. Monitor closely for 24 hours