# Project Structure Guide

## Directory Organization

This document provides a comprehensive overview of the MMORPG Server Engine project structure, following CLAUDE.md methodology principles.

## Root Directory Structure

```
cpp-multiplayer-game-server/
├── README.md                 # Main project documentation
├── CLAUDE.md                 # Development methodology guide
├── API_DOCUMENTATION.md      # API reference
├── CHANGELOG.md             # Version history
├── CMakeLists.txt           # Build configuration
├── Dockerfile               # Container configuration
├── docker-compose.yml       # Multi-service deployment
├── conanfile.txt           # C++ dependencies
├── config/                  # Configuration files
├── docs/                    # Comprehensive documentation
├── src/                     # Source code
├── tests/                   # Test suite
├── versions/               # Development snapshots
├── k8s/                    # Kubernetes deployment
├── monitoring/             # Observability stack
├── scripts/                # Automation scripts
├── proto/                  # Protocol Buffer definitions
├── sql/                    # Database schemas
├── nginx/                  # Load balancer configuration
├── docker/                 # Docker-related files
├── client/                 # Test client applications
├── knowledge/              # Educational materials
└── interview/              # Interview preparation
```

## Core Source Code (`src/`)

### System Architecture

```
src/
├── core/                   # Core engine systems
│   ├── auth/              # Authentication & authorization
│   ├── cache/             # Redis clustering & caching
│   ├── config/            # Configuration management
│   ├── database/          # Database abstraction & sharding
│   ├── ecs/               # Entity-Component-System
│   ├── logging/           # Structured logging
│   ├── memory/            # Memory management & pooling
│   ├── monitoring/        # Metrics & performance
│   ├── network/           # Network protocols & sessions
│   ├── security/          # Security hardening
│   ├── simd/              # SIMD optimizations
│   ├── threading/         # Concurrent programming
│   ├── concepts/          # C++20 concepts
│   ├── algorithms/        # Optimized algorithms
│   └── utils/             # Utility functions
├── game/                   # Game-specific logic
│   ├── ai/                # AI & behavior systems
│   ├── combat/            # Combat mechanics
│   ├── components/        # ECS components
│   ├── economy/           # Economic systems
│   ├── guild/             # Guild management
│   ├── handlers/          # Protocol handlers
│   ├── items/             # Item systems
│   ├── player/            # Player management
│   ├── pvp/               # PvP systems
│   ├── quest/             # Quest systems
│   ├── skills/            # Skill systems
│   ├── social/            # Social features
│   ├── stats/             # Character statistics
│   ├── status/            # Status effects
│   ├── systems/           # ECS systems
│   ├── world/             # World management
│   └── achievement/       # Achievement system
├── database/               # Database layer
│   ├── cache_manager.h    # Caching strategies
│   ├── connection_pool.h  # Connection pooling
│   ├── database_sharding_manager.h # Database sharding
│   ├── query_optimizer.h  # Query optimization
│   └── read_replica_manager.h # Read replicas
├── network/                # Advanced networking
│   ├── global_load_balancer.h # Global load balancing
│   ├── quic_protocol_handler.h # QUIC protocol
│   ├── advanced_networking.h # Network optimizations
│   ├── client_prediction.h # Client prediction
│   └── lag_compensation.h # Lag compensation
├── analytics/              # Real-time analytics
│   └── realtime_analytics_engine.h # Analytics engine
├── server/                 # Server applications
│   ├── game/              # Game server
│   ├── login/             # Login server
│   ├── chat/              # Chat server
│   └── db/                # Database server
├── monitoring/             # System monitoring
│   ├── crash_handler.h    # Crash handling
│   └── ab_testing.h       # A/B testing
├── testing/                # Performance testing
│   ├── performance_testing.h # Performance tests
│   └── coroutine_performance_test.h # Coroutine tests
├── tools/                  # Administrative tools
│   ├── admin/             # Admin interface
│   ├── loadtest/          # Load testing
│   └── monitor/           # Monitoring tools
├── housing/                # Player housing
├── matchmaking/            # Matchmaking service
├── npc/                    # NPC systems
├── quest/                  # Dynamic quests
├── ranking/                # Ranking & leaderboards
├── rewards/                # Reward systems
├── tournament/             # Tournament system
├── ui/                     # User interface
└── optimization/           # Final optimizations
```

## Documentation Structure (`docs/`)

### Educational Documentation

```
docs/
├── development/            # Development process
│   ├── DEVELOPMENT_JOURNEY.md     # Complete development history
│   ├── DEVELOPMENT_JOURNEY_ko.md  # Korean version
│   ├── PROJECT_SUMMARY.md         # Project overview
│   └── TEST_JOURNEY.md             # Testing methodology
├── guides/                 # Implementation guides
│   ├── DEPLOYMENT_GUIDE.md        # Production deployment
│   ├── DEVOPS_GUIDE.md            # DevOps practices
│   ├── SECURITY_GUIDE.md          # Security implementation
│   ├── LEARNING_GUIDE.md          # Learning path
│   ├── KNOWLEDGE_ENHANCEMENT_GUIDE.md # Knowledge enhancement
│   └── FINAL_EXECUTION_GUIDE.md   # Execution guide
├── operations/             # Operational documentation
│   ├── DATABASE_SCHEMA.md          # Database design
│   └── PRODUCTION_REALITY_CHECK.md # Production analysis
└── roadmaps/               # Future planning
    ├── COMPLETE_ROADMAP.md         # Complete roadmap
    └── FutureExtensionPlan.md      # Extension plans
```

## Test Suite (`tests/`)

### Comprehensive Testing

```
tests/
├── unit/                   # Unit tests
│   ├── test_combat_system.cpp     # Combat testing
│   ├── test_ecs_system.cpp        # ECS testing
│   ├── test_networking.cpp        # Network testing
│   └── test_spatial_indexing.cpp  # Spatial testing
├── integration/            # Integration tests
│   └── test_game_flow.cpp          # End-to-end testing
├── performance/            # Performance tests
│   └── test_load_capacity.cpp     # Load testing
├── pvp/                    # PvP system tests
│   ├── arena_test_scenarios.cpp   # Arena testing
│   ├── openworld_test_scenarios.cpp # Open world testing
│   ├── pvp_integration_test.cpp   # PvP integration
│   └── run_pvp_tests.sh           # Test runner
├── load_test/              # Load testing suite
│   ├── load_test_client.cpp       # Load test client
│   ├── load_test_plan.md          # Test planning
│   └── main.cpp                   # Test entry point
└── tier1_integration_test.cpp # Tier 1 integration tests
```

## Version Control (`versions/`)

### Development Snapshots

```
versions/
├── mvp0-infrastructure/    # Initial infrastructure
├── mvp1-networking/        # Basic networking
├── mvp2-ecs-basic/        # Basic ECS
├── mvp2-ecs-optimized/    # Optimized ECS
├── mvp3-world-grid/       # Grid-based world
├── mvp3-world-octree/     # Octree-based world
├── mvp4-combat-action/    # Action combat
├── mvp4-combat-targeted/  # Targeted combat
├── mvp5-guild-wars/       # Guild war systems
├── mvp5-pvp-systems/      # PvP systems
├── mvp6-deployment/       # Deployment configuration
├── mvp6-bare-metal/       # Bare metal deployment
├── mvp7-infrastructure-integration/ # Infrastructure integration
├── mvp14-world-systems/   # World systems
├── mvp16-ui-system-complete/ # UI completion
├── phase-095-hud-elements/ # HUD elements
├── phase-096-inventory-ui/ # Inventory UI
├── phase-097-chat-interface/ # Chat interface
├── phase-098-map-interface/ # Map interface
├── phase-099-crash-handler/ # Crash handling
├── phase-100-ab-testing/  # A/B testing
├── phase-101-matchmaking/ # Matchmaking
├── phase-102-ranking-system/ # Ranking system
├── phase-103-leaderboards/ # Leaderboards
├── phase-104-arena-system/ # Arena system
├── phase-105-tournament-system/ # Tournament system
├── phase-106-rewards-system/ # Reward system
├── phase-107-db-partitioning/ # Database partitioning
├── phase-108-read-replicas/ # Read replicas
├── phase-109-cache-optimization/ # Cache optimization
├── phase-110-query-optimization/ # Query optimization
├── phase-112-database-monitoring/ # Database monitoring
├── phase-113-player-housing/ # Player housing
├── phase-114-decoration-system/ # Decoration system
├── phase-115-furniture-crafting/ # Furniture crafting
├── phase-116-housing-storage/ # Housing storage
├── phase-117-housing-permissions/ # Housing permissions
├── phase-118-neighborhood-system/ # Neighborhood system
├── phase-120-npc-dialogue/ # NPC dialogue
├── phase-121-dynamic-quest/ # Dynamic quests
├── phase-122-advanced-networking/ # Advanced networking
├── phase-123-client-prediction/ # Client prediction
├── phase-125-final-optimization/ # Final optimization
├── phase-126-performance-testing/ # Performance testing
├── phase-127-security-hardening/ # Security hardening
├── phase-128-coroutines-basic/ # Basic coroutines
└── phase-130-memory-optimization/ # Memory optimization
```

## Knowledge Base (`knowledge/`)

### Educational Resources

```
knowledge/
├── core/                   # Core concepts
│   ├── authentication_jwt_implementation.md
│   ├── combat_system_architecture.md
│   ├── game_server_ecs_implementation.md
│   ├── network_protocol_implementation.md
│   ├── performance_optimization_guide.md
│   ├── server_monitoring_implementation.md
│   └── spatial_indexing_guide.md
├── reference/              # Reference materials
│   ├── cpp_basics_for_game_server.md
│   ├── network_programming_basics.md
│   ├── performance_optimization_basics.md
│   ├── multithreading_basics.md
│   ├── database_design_optimization_guide.md
│   ├── ecs_architecture_system.md
│   ├── event_driven_architecture_guide.md
│   ├── microservices_architecture_guide.md
│   ├── lockfree_programming_guide.md
│   ├── advanced_networking_optimization.md
│   ├── advanced_database_operations.md
│   ├── advanced_security_compliance.md
│   ├── ai_game_logic_engine.md
│   ├── cloud_native_game_server.md
│   ├── global_service_operations.md
│   ├── next_gen_game_tech.md
│   ├── performance_engineering_advanced.md
│   ├── practical_setup_guide.md
│   ├── realtime_analytics_bigdata_pipeline.md
│   ├── realtime_physics_synchronization.md
│   ├── redis_cluster_caching_master.md
│   ├── security_authentication_guide.md
│   └── system_separation_world_management.md
└── appendix/               # Additional resources
    ├── protocol_buffers_implementation_guide.md
    ├── realtime_pvp_systems_guide.md
    ├── server_monitoring_metrics_guide.md
    └── spatial_systems_performance_comparison.md
```

## Infrastructure & Operations

### Deployment Configuration

```
k8s/                        # Kubernetes deployment
├── base/                   # Base configuration
│   ├── configmap.yaml     # Configuration maps
│   ├── deployment.yaml    # Application deployment
│   ├── namespace.yaml     # Namespace definition
│   └── service.yaml       # Service definition
└── overlays/               # Environment overlays
    ├── dev/                # Development environment
    └── prod/               # Production environment
        ├── deployment-patch.yaml
        └── kustomization.yaml

monitoring/                 # Monitoring stack
├── prometheus.yml          # Prometheus configuration
├── alerts.yml             # Alert rules
└── grafana/               # Grafana configuration
    └── provisioning/
        ├── dashboards/
        └── datasources/

nginx/                      # Load balancer
└── nginx.conf             # Nginx configuration

scripts/                    # Automation scripts
└── deploy.sh              # Deployment script
```

## Development Methodology Compliance

This project structure follows **CLAUDE.md methodology** principles:

### 1. Two-Document System ✅
- **README.md**: Production-ready documentation
- **DEVELOPMENT_JOURNEY.md**: Complete development history

### 2. MVP-Driven Development ✅
- **versions/**: Snapshot of each MVP milestone
- **Incremental progress**: 138 development phases

### 3. Sequence Tracking ✅
- **1,000 sequence numbers**: Logical implementation flow
- **Clear progression**: From basic concepts to enterprise-grade

### 4. Learning-Friendly Structure ✅
- **knowledge/**: Educational resources
- **docs/guides/**: Implementation guides
- **interview/**: Interview preparation materials

## Key Features by Directory

### Core Engine (`src/core/`)
- **Authentication**: JWT-based auth with hardening
- **Caching**: Redis clustering with automatic failover
- **Database**: Sharding with 2PC transactions
- **ECS**: High-performance entity-component system
- **Network**: Asynchronous I/O with QUIC support
- **Security**: Enterprise-grade protection
- **SIMD**: 794% performance optimization

### Game Logic (`src/game/`)
- **AI**: Adaptive player behavior learning
- **Combat**: Real-time deterministic systems
- **Social**: Guild wars, chat, friend systems
- **World**: Dynamic spawning, weather, events
- **Economy**: Auction house, trading systems

### Infrastructure
- **Analytics**: Real-time monitoring with anomaly detection
- **Load Balancing**: Geographic traffic distribution
- **Monitoring**: Prometheus/Grafana integration
- **Testing**: Comprehensive test suite with 100+ scenarios

## Navigation Guide

### For Learners
1. Start with `README.md` for overview
2. Review `docs/development/DEVELOPMENT_JOURNEY.md` for complete learning path
3. Explore `knowledge/` for educational materials
4. Practice with `tests/` for hands-on experience

### For Developers
1. Study `src/core/` for engine architecture
2. Examine `src/game/` for game-specific implementation
3. Review `docs/guides/` for implementation patterns
4. Use `versions/` to understand evolution

### For Operations
1. Check `k8s/` for deployment configuration
2. Review `monitoring/` for observability setup
3. Use `scripts/` for automation
4. Follow `docs/guides/DEPLOYMENT_GUIDE.md`

This structure represents **1,000+ sequence-tracked decisions** implementing enterprise-grade game server architecture with complete educational value.