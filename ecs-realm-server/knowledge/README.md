# C++ Multiplayer Game Server Knowledge Base

## 🚀 빠른 시작 가이드
- **처음이신가요?** → [📚 게임 서버 학습 로드맵](GAME_SERVER_LEARNING_ROADMAP.md)을 먼저 확인하세요!
- **필수 문서만 보기** → [📋 핵심 문서 20선](ESSENTIAL_DOCUMENTS_LIST.md)으로 효율적 학습
- **실습이 필요해요** → [🎮 미니 프로젝트 가이드](MINI_PROJECTS_GUIDE.md)로 실력 향상
- **문제가 생겼어요** → [🔧 트러블슈팅 가이드](GAME_SERVER_TROUBLESHOOTING.md)에서 해결책 찾기

## 📁 Folder Structure

### core/ - Project Core Documentation (7 documents)
Contains the essential documentation specific to this game server implementation. Start here to understand the actual codebase.

- **[game_server_ecs_implementation.md](core/game_server_ecs_implementation.md)** - Entity Component System architecture with optimized variants
- **[spatial_indexing_guide.md](core/spatial_indexing_guide.md)** - Grid and Octree spatial systems for efficient queries
- **[network_protocol_implementation.md](core/network_protocol_implementation.md)** - TCP/Protobuf networking with Boost.Asio
- **[authentication_jwt_implementation.md](core/authentication_jwt_implementation.md)** - JWT auth with Redis session management
- **[combat_system_architecture.md](core/combat_system_architecture.md)** - Action and targeted combat implementations
- **[performance_optimization_guide.md](core/performance_optimization_guide.md)** - Memory pools, SIMD, and optimization techniques
- **[server_monitoring_implementation.md](core/server_monitoring_implementation.md)** - Prometheus/Grafana monitoring setup

### reference/ - General Reference Documentation (24 documents)
General programming concepts, C++ techniques, and theoretical knowledge. Use these for learning and understanding underlying concepts.

**Key Topics:**
- C++ basics and advanced features
- Multithreading and synchronization
- Network programming concepts
- Database design patterns
- Performance engineering
- Security best practices
- Cloud deployment strategies

### appendix/ - Supplementary Materials (4 documents)
Additional implementation guides and comparisons that complement the core documentation.

- Protocol Buffers implementation examples
- Real-time PvP system designs
- Server monitoring metrics guide
- Spatial system performance comparisons

## 🎯 Documentation Navigation Guide

### For Understanding the Project
1. Start with **[game_server_ecs_implementation.md](core/game_server_ecs_implementation.md)** to understand the architecture
2. Read **[network_protocol_implementation.md](core/network_protocol_implementation.md)** for networking details
3. Explore specific systems (combat, spatial, auth) based on your interest

### For Learning Game Server Development
1. Begin with reference documents on C++ basics and multithreading
2. Study network programming concepts in reference/
3. Apply knowledge by reading the core implementation documents

### For Performance Optimization
1. **[performance_optimization_guide.md](core/performance_optimization_guide.md)** - Start here
2. Reference documents on lock-free programming and advanced techniques
3. **[server_monitoring_implementation.md](core/server_monitoring_implementation.md)** - Measure improvements

## 📝 Documentation Standards

### Core Documents
- Always reference actual code locations (file:line)
- Include performance metrics and benchmarks
- Document design decisions and trade-offs
- Provide debugging tips from development experience

### Reference Documents
- Focus on general concepts and best practices
- Provide multiple implementation examples
- Include external references and further reading
- Keep language/framework agnostic where possible

### Cross-References
- Core documents link to relevant reference materials
- Reference documents avoid linking to project-specific content
- Use relative links for navigation

## 🔄 Maintenance Guidelines

### Adding New Documentation
- **Core**: Only for implemented features with code references
- **Reference**: For reusable knowledge across projects
- **Appendix**: For experimental or supplementary content

### Updating Existing Documentation
- Keep code references up-to-date
- Update performance metrics with new optimizations
- Add lessons learned from production experience
- Maintain backward compatibility references

## 📊 Quick Reference

### Project Statistics
- **Architecture**: ECS-based with spatial indexing
- **Performance**: 5,000+ concurrent players
- **Technology**: C++17, Boost.Asio, Protocol Buffers
- **Database**: MySQL with Redis caching
- **Monitoring**: Prometheus + Grafana

### Key Implementation Files
- ECS: `src/core/ecs/`
- Networking: `src/core/network/`
- Game Systems: `src/game/systems/`
- Combat: `src/game/combat/`
- Monitoring: `src/core/monitoring/`

---

*This knowledge base is continuously updated as the project evolves. For the latest implementation details, always refer to the source code.*