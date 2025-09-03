#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <algorithm>
#include <random>
#include <queue>
#include <future>

namespace mmorpg::network {

// [SEQUENCE: 976] 글로벌 로드 밸런서
class GlobalLoadBalancer {
public:
    struct ServerNode {
        std::string server_id;
        std::string hostname;
        std::string ip_address;
        uint16_t port;
        std::string region; // "us-east", "eu-west", "asia-pacific"
        std::string datacenter;
        
        // 성능 지표
        std::atomic<double> cpu_usage{0.0};
        std::atomic<double> memory_usage{0.0};
        std::atomic<uint32_t> active_connections{0};
        std::atomic<uint32_t> max_connections{1000};
        std::atomic<double> average_latency_ms{0.0};
        std::atomic<bool> is_healthy{true};
        std::atomic<bool> is_maintenance{false};
        
        // 지리적 정보
        double latitude{0.0};
        double longitude{0.0};
        
        // 가중치 및 우선순위
        uint32_t weight{100}; // 기본 가중치
        uint32_t priority{1}; // 낮을수록 높은 우선순위
        
        std::chrono::steady_clock::time_point last_health_check;
        std::chrono::steady_clock::time_point last_update;
        
        ServerNode(const std::string& id, const std::string& host, uint16_t p, const std::string& reg)
            : server_id(id), hostname(host), port(p), region(reg) {
            last_health_check = std::chrono::steady_clock::now();
            last_update = std::chrono::steady_clock::now();
        }
        
        double GetLoadScore() const {
            double connection_load = static_cast<double>(active_connections.load()) / max_connections.load();
            double resource_load = (cpu_usage.load() + memory_usage.load()) / 200.0; // 0.0 ~ 1.0
            double latency_penalty = std::min(1.0, average_latency_ms.load() / 1000.0); // 1초 기준
            
            return (connection_load * 0.4) + (resource_load * 0.4) + (latency_penalty * 0.2);
        }
        
        bool IsAvailable() const {
            return is_healthy.load() && !is_maintenance.load() && 
                   active_connections.load() < max_connections.load();
        }
    };

    struct ClientLocation {
        std::string client_id;
        std::string ip_address;
        double latitude{0.0};
        double longitude{0.0};
        std::string estimated_region;
        std::string isp;
        std::chrono::steady_clock::time_point last_seen;
        
        // 연결 기록
        std::string last_assigned_server;
        std::vector<std::string> connection_history;
        uint32_t total_connections{0};
        
        ClientLocation(const std::string& id, const std::string& ip)
            : client_id(id), ip_address(ip), last_seen(std::chrono::steady_clock::now()) {}
    };

    enum class LoadBalancingStrategy {
        ROUND_ROBIN,      // 순환 방식
        LEAST_CONNECTIONS, // 최소 연결 수
        WEIGHTED_ROUND_ROBIN, // 가중치 기반 순환
        GEOGRAPHIC,       // 지리적 근접성
        LEAST_RESPONSE_TIME, // 최소 응답 시간
        RESOURCE_BASED,   // 리소스 사용량 기반
        INTELLIGENT       // AI 기반 최적 선택
    };

    struct LoadBalancerConfig {
        LoadBalancingStrategy primary_strategy{LoadBalancingStrategy::INTELLIGENT};
        LoadBalancingStrategy fallback_strategy{LoadBalancingStrategy::LEAST_CONNECTIONS};
        
        std::chrono::seconds health_check_interval{30};
        std::chrono::seconds failover_timeout{10};
        double max_acceptable_latency_ms{500.0};
        
        bool enable_sticky_sessions{true};
        std::chrono::minutes session_affinity_duration{30};
        
        bool enable_geographic_routing{true};
        double geographic_preference_weight{0.3};
        
        bool enable_predictive_scaling{true};
        double load_threshold_scale_up{0.8};
        double load_threshold_scale_down{0.3};
        
        size_t max_servers_per_region{10};
        size_t min_servers_per_region{2};
    };

    explicit GlobalLoadBalancer(const LoadBalancerConfig& config = {})
        : config_(config), is_running_(false) {
        InitializeStrategies();
    }

    ~GlobalLoadBalancer() {
        Shutdown();
    }

    // [SEQUENCE: 977] 서버 노드 관리
    bool RegisterServer(const ServerNode& server) {
        std::lock_guard<std::mutex> lock(servers_mutex_);
        
        servers_[server.server_id] = std::make_shared<ServerNode>(server);
        
        // 지역별 서버 그룹 업데이트
        region_servers_[server.region].push_back(server.server_id);
        
        return true;
    }

    bool UnregisterServer(const std::string& server_id) {
        std::lock_guard<std::mutex> lock(servers_mutex_);
        
        auto it = servers_.find(server_id);
        if (it == servers_.end()) {
            return false;
        }
        
        const std::string& region = it->second->region;
        
        // 지역별 서버 목록에서 제거
        auto& region_list = region_servers_[region];
        region_list.erase(std::remove(region_list.begin(), region_list.end(), server_id), region_list.end());
        
        servers_.erase(it);
        return true;
    }

    void UpdateServerMetrics(const std::string& server_id, double cpu, double memory, 
                           uint32_t connections, double latency) {
        std::lock_guard<std::mutex> lock(servers_mutex_);
        
        auto it = servers_.find(server_id);
        if (it != servers_.end()) {
            auto& server = it->second;
            server->cpu_usage.store(cpu);
            server->memory_usage.store(memory);
            server->active_connections.store(connections);
            server->average_latency_ms.store(latency);
            server->last_update = std::chrono::steady_clock::now();
        }
    }

    void SetServerHealthStatus(const std::string& server_id, bool is_healthy) {
        std::lock_guard<std::mutex> lock(servers_mutex_);
        
        auto it = servers_.find(server_id);
        if (it != servers_.end()) {
            it->second->is_healthy.store(is_healthy);
            it->second->last_health_check = std::chrono::steady_clock::now();
        }
    }

    // [SEQUENCE: 978] 클라이언트 요청 라우팅
    struct RoutingResult {
        bool success{false};
        std::string selected_server_id;
        std::string server_hostname;
        uint16_t server_port{0};
        std::string routing_reason;
        double estimated_latency_ms{0.0};
        LoadBalancingStrategy strategy_used;
    };

    RoutingResult RouteClient(const std::string& client_id, const std::string& client_ip = "",
                             const std::string& preferred_region = "") {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        
        // 클라이언트 정보 업데이트 또는 생성
        auto& client = GetOrCreateClient(client_id, client_ip);
        client.last_seen = std::chrono::steady_clock::now();
        
        RoutingResult result;
        result.strategy_used = config_.primary_strategy;
        
        // Sticky session 확인
        if (config_.enable_sticky_sessions && !client.last_assigned_server.empty()) {
            auto sticky_result = TryStickySessions(client);
            if (sticky_result.success) {
                return sticky_result;
            }
        }
        
        // 지리적 정보 기반 클라이언트 위치 추정
        if (config_.enable_geographic_routing && !client_ip.empty()) {
            EstimateClientLocation(client, client_ip);
        }
        
        // 선택된 전략에 따라 서버 선택
        std::vector<std::string> candidate_servers = GetCandidateServers(client, preferred_region);
        
        if (candidate_servers.empty()) {
            result.routing_reason = "No available servers";
            return result;
        }
        
        std::string selected_server = SelectOptimalServer(candidate_servers, client, config_.primary_strategy);
        
        if (selected_server.empty()) {
            // 폴백 전략 시도
            selected_server = SelectOptimalServer(candidate_servers, client, config_.fallback_strategy);
            result.strategy_used = config_.fallback_strategy;
        }
        
        if (!selected_server.empty()) {
            auto server_it = servers_.find(selected_server);
            if (server_it != servers_.end()) {
                result.success = true;
                result.selected_server_id = selected_server;
                result.server_hostname = server_it->second->hostname;
                result.server_port = server_it->second->port;
                result.estimated_latency_ms = server_it->second->average_latency_ms.load();
                result.routing_reason = GetStrategyDescription(result.strategy_used);
                
                // 클라이언트 연결 기록 업데이트
                client.last_assigned_server = selected_server;
                client.connection_history.push_back(selected_server);
                client.total_connections++;
                
                // 연결 기록 크기 제한
                if (client.connection_history.size() > 10) {
                    client.connection_history.erase(client.connection_history.begin());
                }
                
                // 서버 연결 수 증가
                server_it->second->active_connections.fetch_add(1);
            }
        }
        
        return result;
    }

    // [SEQUENCE: 979] 지역별 서버 스케일링
    struct ScalingRecommendation {
        std::string region;
        std::string action; // "scale_up", "scale_down", "maintain"
        uint32_t recommended_server_count;
        uint32_t current_server_count;
        double current_load_percentage;
        std::string reasoning;
    };

    std::vector<ScalingRecommendation> AnalyzeScalingNeeds() {
        std::lock_guard<std::mutex> lock(servers_mutex_);
        
        std::vector<ScalingRecommendation> recommendations;
        
        for (const auto& [region, server_ids] : region_servers_) {
            ScalingRecommendation rec;
            rec.region = region;
            rec.current_server_count = server_ids.size();
            
            // 지역별 서버 로드 분석
            double total_load = 0.0;
            uint32_t healthy_servers = 0;
            
            for (const auto& server_id : server_ids) {
                auto server_it = servers_.find(server_id);
                if (server_it != servers_.end() && server_it->second->IsAvailable()) {
                    total_load += server_it->second->GetLoadScore();
                    healthy_servers++;
                }
            }
            
            if (healthy_servers == 0) {
                rec.action = "scale_up";
                rec.recommended_server_count = config_.min_servers_per_region;
                rec.reasoning = "No healthy servers in region";
                recommendations.push_back(rec);
                continue;
            }
            
            double average_load = total_load / healthy_servers;
            rec.current_load_percentage = average_load * 100.0;
            
            // 스케일링 결정
            if (average_load > config_.load_threshold_scale_up) {
                rec.action = "scale_up";
                rec.recommended_server_count = std::min(static_cast<uint32_t>(healthy_servers * 1.5), 
                                                       config_.max_servers_per_region);
                rec.reasoning = "High load detected: " + std::to_string(average_load * 100) + "%";
            } else if (average_load < config_.load_threshold_scale_down && 
                      healthy_servers > config_.min_servers_per_region) {
                rec.action = "scale_down";
                rec.recommended_server_count = std::max(static_cast<uint32_t>(healthy_servers * 0.8), 
                                                       config_.min_servers_per_region);
                rec.reasoning = "Low load detected: " + std::to_string(average_load * 100) + "%";
            } else {
                rec.action = "maintain";
                rec.recommended_server_count = healthy_servers;
                rec.reasoning = "Load within acceptable range";
            }
            
            recommendations.push_back(rec);
        }
        
        return recommendations;
    }

    // [SEQUENCE: 980] 로드 밸런서 통계
    struct LoadBalancerStats {
        uint32_t total_servers{0};
        uint32_t healthy_servers{0};
        uint32_t total_connections{0};
        double average_server_load{0.0};
        
        std::unordered_map<std::string, uint32_t> servers_per_region;
        std::unordered_map<std::string, uint32_t> connections_per_region;
        std::unordered_map<LoadBalancingStrategy, uint32_t> strategy_usage_count;
        
        uint64_t total_routing_requests{0};
        uint64_t successful_routings{0};
        double routing_success_rate{0.0};
        
        std::chrono::steady_clock::time_point last_updated;
    };

    LoadBalancerStats GetStatistics() {
        std::lock_guard<std::mutex> lock(servers_mutex_);
        
        LoadBalancerStats stats;
        stats.last_updated = std::chrono::steady_clock::now();
        
        double total_load = 0.0;
        
        for (const auto& [server_id, server] : servers_) {
            stats.total_servers++;
            
            if (server->IsAvailable()) {
                stats.healthy_servers++;
                total_load += server->GetLoadScore();
            }
            
            stats.total_connections += server->active_connections.load();
            stats.servers_per_region[server->region]++;
            stats.connections_per_region[server->region] += server->active_connections.load();
        }
        
        if (stats.healthy_servers > 0) {
            stats.average_server_load = total_load / stats.healthy_servers;
        }
        
        // 라우팅 통계
        stats.total_routing_requests = total_routing_requests_.load();
        stats.successful_routings = successful_routings_.load();
        
        if (stats.total_routing_requests > 0) {
            stats.routing_success_rate = static_cast<double>(stats.successful_routings) / stats.total_routing_requests;
        }
        
        stats.strategy_usage_count = strategy_usage_count_;
        
        return stats;
    }

    void StartLoadBalancer() {
        if (is_running_) return;
        
        is_running_ = true;
        
        // 헬스체크 스레드
        health_check_thread_ = std::thread([this]() {
            while (is_running_) {
                PerformHealthChecks();
                std::this_thread::sleep_for(config_.health_check_interval);
            }
        });
        
        // 자동 스케일링 분석 스레드
        scaling_thread_ = std::thread([this]() {
            while (is_running_) {
                if (config_.enable_predictive_scaling) {
                    auto recommendations = AnalyzeScalingNeeds();
                    ProcessScalingRecommendations(recommendations);
                }
                std::this_thread::sleep_for(std::chrono::minutes(5));
            }
        });
    }

    void Shutdown() {
        is_running_ = false;
        
        if (health_check_thread_.joinable()) {
            health_check_thread_.join();
        }
        
        if (scaling_thread_.joinable()) {
            scaling_thread_.join();
        }
    }

private:
    LoadBalancerConfig config_;
    std::atomic<bool> is_running_;
    
    // 서버 관리
    std::unordered_map<std::string, std::shared_ptr<ServerNode>> servers_;
    std::unordered_map<std::string, std::vector<std::string>> region_servers_;
    std::mutex servers_mutex_;
    
    // 클라이언트 관리
    std::unordered_map<std::string, ClientLocation> clients_;
    std::mutex clients_mutex_;
    
    // 로드 밸런싱 전략
    std::unordered_map<LoadBalancingStrategy, std::function<std::string(const std::vector<std::string>&, const ClientLocation&)>> strategies_;
    
    // 통계
    std::atomic<uint64_t> total_routing_requests_{0};
    std::atomic<uint64_t> successful_routings_{0};
    std::unordered_map<LoadBalancingStrategy, uint32_t> strategy_usage_count_;
    
    // 백그라운드 스레드
    std::thread health_check_thread_;
    std::thread scaling_thread_;
    
    // 라운드 로빈 상태
    std::unordered_map<std::string, std::atomic<size_t>> round_robin_counters_;

    // [SEQUENCE: 981] 로드 밸런싱 전략 구현
    void InitializeStrategies() {
        strategies_[LoadBalancingStrategy::ROUND_ROBIN] = [this](const std::vector<std::string>& candidates, const ClientLocation& client) {
            return SelectRoundRobin(candidates);
        };
        
        strategies_[LoadBalancingStrategy::LEAST_CONNECTIONS] = [this](const std::vector<std::string>& candidates, const ClientLocation& client) {
            return SelectLeastConnections(candidates);
        };
        
        strategies_[LoadBalancingStrategy::WEIGHTED_ROUND_ROBIN] = [this](const std::vector<std::string>& candidates, const ClientLocation& client) {
            return SelectWeightedRoundRobin(candidates);
        };
        
        strategies_[LoadBalancingStrategy::GEOGRAPHIC] = [this](const std::vector<std::string>& candidates, const ClientLocation& client) {
            return SelectGeographic(candidates, client);
        };
        
        strategies_[LoadBalancingStrategy::LEAST_RESPONSE_TIME] = [this](const std::vector<std::string>& candidates, const ClientLocation& client) {
            return SelectLeastResponseTime(candidates);
        };
        
        strategies_[LoadBalancingStrategy::RESOURCE_BASED] = [this](const std::vector<std::string>& candidates, const ClientLocation& client) {
            return SelectResourceBased(candidates);
        };
        
        strategies_[LoadBalancingStrategy::INTELLIGENT] = [this](const std::vector<std::string>& candidates, const ClientLocation& client) {
            return SelectIntelligent(candidates, client);
        };
    }

    ClientLocation& GetOrCreateClient(const std::string& client_id, const std::string& client_ip) {
        auto it = clients_.find(client_id);
        if (it != clients_.end()) {
            return it->second;
        }
        
        clients_[client_id] = ClientLocation(client_id, client_ip);
        return clients_[client_id];
    }

    RoutingResult TryStickySessions(const ClientLocation& client) {
        RoutingResult result;
        
        if (client.last_assigned_server.empty()) {
            return result;
        }
        
        auto now = std::chrono::steady_clock::now();
        if (now - client.last_seen > config_.session_affinity_duration) {
            return result; // 세션 만료
        }
        
        auto server_it = servers_.find(client.last_assigned_server);
        if (server_it != servers_.end() && server_it->second->IsAvailable()) {
            result.success = true;
            result.selected_server_id = client.last_assigned_server;
            result.server_hostname = server_it->second->hostname;
            result.server_port = server_it->second->port;
            result.routing_reason = "Sticky session";
            result.strategy_used = LoadBalancingStrategy::ROUND_ROBIN; // 임시값
            result.estimated_latency_ms = server_it->second->average_latency_ms.load();
        }
        
        return result;
    }

    void EstimateClientLocation(ClientLocation& client, const std::string& ip) {
        // IP 기반 지리적 위치 추정 (GeoIP 시뮬레이션)
        // 실제 구현에서는 MaxMind GeoLite2 같은 서비스 사용
        
        if (ip.substr(0, 7) == "192.168" || ip.substr(0, 3) == "10." || ip.substr(0, 7) == "172.16") {
            client.estimated_region = "local";
            return;
        }
        
        // 간단한 IP 기반 지역 추정
        uint32_t ip_hash = std::hash<std::string>{}(ip);
        switch (ip_hash % 3) {
            case 0: 
                client.estimated_region = "us-east";
                client.latitude = 40.7128;
                client.longitude = -74.0060;
                break;
            case 1:
                client.estimated_region = "eu-west";
                client.latitude = 51.5074;
                client.longitude = -0.1278;
                break;
            case 2:
                client.estimated_region = "asia-pacific";
                client.latitude = 35.6762;
                client.longitude = 139.6503;
                break;
        }
    }

    std::vector<std::string> GetCandidateServers(const ClientLocation& client, const std::string& preferred_region) {
        std::vector<std::string> candidates;
        
        std::string target_region = preferred_region.empty() ? client.estimated_region : preferred_region;
        
        // 선호 지역의 서버들 먼저 추가
        if (!target_region.empty() && region_servers_.count(target_region)) {
            for (const auto& server_id : region_servers_[target_region]) {
                auto server_it = servers_.find(server_id);
                if (server_it != servers_.end() && server_it->second->IsAvailable()) {
                    candidates.push_back(server_id);
                }
            }
        }
        
        // 다른 지역 서버들도 추가 (폴백용)
        for (const auto& [region, server_ids] : region_servers_) {
            if (region == target_region) continue;
            
            for (const auto& server_id : server_ids) {
                auto server_it = servers_.find(server_id);
                if (server_it != servers_.end() && server_it->second->IsAvailable()) {
                    candidates.push_back(server_id);
                }
            }
        }
        
        return candidates;
    }

    std::string SelectOptimalServer(const std::vector<std::string>& candidates, 
                                   const ClientLocation& client, 
                                   LoadBalancingStrategy strategy) {
        if (candidates.empty()) return "";
        
        total_routing_requests_.fetch_add(1);
        strategy_usage_count_[strategy]++;
        
        auto strategy_it = strategies_.find(strategy);
        if (strategy_it != strategies_.end()) {
            std::string result = strategy_it->second(candidates, client);
            if (!result.empty()) {
                successful_routings_.fetch_add(1);
            }
            return result;
        }
        
        return candidates[0]; // 폴백
    }

    // [SEQUENCE: 982] 개별 로드 밸런싱 전략들
    std::string SelectRoundRobin(const std::vector<std::string>& candidates) {
        static std::atomic<size_t> counter{0};
        size_t index = counter.fetch_add(1) % candidates.size();
        return candidates[index];
    }

    std::string SelectLeastConnections(const std::vector<std::string>& candidates) {
        std::string best_server;
        uint32_t min_connections = std::numeric_limits<uint32_t>::max();
        
        for (const auto& server_id : candidates) {
            auto server_it = servers_.find(server_id);
            if (server_it != servers_.end()) {
                uint32_t connections = server_it->second->active_connections.load();
                if (connections < min_connections) {
                    min_connections = connections;
                    best_server = server_id;
                }
            }
        }
        
        return best_server;
    }

    std::string SelectWeightedRoundRobin(const std::vector<std::string>& candidates) {
        // 가중치 기반 선택
        uint32_t total_weight = 0;
        for (const auto& server_id : candidates) {
            auto server_it = servers_.find(server_id);
            if (server_it != servers_.end()) {
                total_weight += server_it->second->weight;
            }
        }
        
        if (total_weight == 0) return SelectRoundRobin(candidates);
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> dist(1, total_weight);
        
        uint32_t random_weight = dist(gen);
        uint32_t cumulative_weight = 0;
        
        for (const auto& server_id : candidates) {
            auto server_it = servers_.find(server_id);
            if (server_it != servers_.end()) {
                cumulative_weight += server_it->second->weight;
                if (random_weight <= cumulative_weight) {
                    return server_id;
                }
            }
        }
        
        return candidates[0];
    }

    std::string SelectGeographic(const std::vector<std::string>& candidates, const ClientLocation& client) {
        if (client.latitude == 0.0 && client.longitude == 0.0) {
            return SelectLeastConnections(candidates);
        }
        
        std::string best_server;
        double min_distance = std::numeric_limits<double>::max();
        
        for (const auto& server_id : candidates) {
            auto server_it = servers_.find(server_id);
            if (server_it != servers_.end()) {
                double distance = CalculateGeographicDistance(
                    client.latitude, client.longitude,
                    server_it->second->latitude, server_it->second->longitude
                );
                
                if (distance < min_distance) {
                    min_distance = distance;
                    best_server = server_id;
                }
            }
        }
        
        return best_server.empty() ? SelectLeastConnections(candidates) : best_server;
    }

    std::string SelectLeastResponseTime(const std::vector<std::string>& candidates) {
        std::string best_server;
        double min_latency = std::numeric_limits<double>::max();
        
        for (const auto& server_id : candidates) {
            auto server_it = servers_.find(server_id);
            if (server_it != servers_.end()) {
                double latency = server_it->second->average_latency_ms.load();
                if (latency < min_latency) {
                    min_latency = latency;
                    best_server = server_id;
                }
            }
        }
        
        return best_server;
    }

    std::string SelectResourceBased(const std::vector<std::string>& candidates) {
        std::string best_server;
        double min_load = std::numeric_limits<double>::max();
        
        for (const auto& server_id : candidates) {
            auto server_it = servers_.find(server_id);
            if (server_it != servers_.end()) {
                double load = server_it->second->GetLoadScore();
                if (load < min_load) {
                    min_load = load;
                    best_server = server_id;
                }
            }
        }
        
        return best_server;
    }

    std::string SelectIntelligent(const std::vector<std::string>& candidates, const ClientLocation& client) {
        // 복합적인 스코어링 기반 선택
        std::string best_server;
        double best_score = std::numeric_limits<double>::max();
        
        for (const auto& server_id : candidates) {
            auto server_it = servers_.find(server_id);
            if (server_it == servers_.end()) continue;
            
            const auto& server = server_it->second;
            
            // 종합 점수 계산
            double load_score = server->GetLoadScore();
            double latency_score = std::min(1.0, server->average_latency_ms.load() / config_.max_acceptable_latency_ms);
            
            double geographic_score = 0.0;
            if (config_.enable_geographic_routing && client.latitude != 0.0) {
                double distance = CalculateGeographicDistance(
                    client.latitude, client.longitude,
                    server->latitude, server->longitude
                );
                geographic_score = std::min(1.0, distance / 20000.0); // 20,000km 기준 정규화
            }
            
            // 가중 평균 계산
            double composite_score = (load_score * 0.4) + 
                                   (latency_score * 0.3) + 
                                   (geographic_score * config_.geographic_preference_weight);
            
            if (composite_score < best_score) {
                best_score = composite_score;
                best_server = server_id;
            }
        }
        
        return best_server;
    }

    // [SEQUENCE: 983] 유틸리티 함수들
    double CalculateGeographicDistance(double lat1, double lon1, double lat2, double lon2) {
        // Haversine 공식으로 두 지점 간 거리 계산 (km)
        const double R = 6371.0; // 지구 반지름 (km)
        
        double lat1_rad = lat1 * M_PI / 180.0;
        double lat2_rad = lat2 * M_PI / 180.0;
        double delta_lat = (lat2 - lat1) * M_PI / 180.0;
        double delta_lon = (lon2 - lon1) * M_PI / 180.0;
        
        double a = std::sin(delta_lat / 2) * std::sin(delta_lat / 2) +
                  std::cos(lat1_rad) * std::cos(lat2_rad) *
                  std::sin(delta_lon / 2) * std::sin(delta_lon / 2);
        
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
        
        return R * c;
    }

    std::string GetStrategyDescription(LoadBalancingStrategy strategy) {
        switch (strategy) {
            case LoadBalancingStrategy::ROUND_ROBIN: return "Round Robin";
            case LoadBalancingStrategy::LEAST_CONNECTIONS: return "Least Connections";
            case LoadBalancingStrategy::WEIGHTED_ROUND_ROBIN: return "Weighted Round Robin";
            case LoadBalancingStrategy::GEOGRAPHIC: return "Geographic Proximity";
            case LoadBalancingStrategy::LEAST_RESPONSE_TIME: return "Least Response Time";
            case LoadBalancingStrategy::RESOURCE_BASED: return "Resource Based";
            case LoadBalancingStrategy::INTELLIGENT: return "Intelligent Composite";
            default: return "Unknown";
        }
    }

    void PerformHealthChecks() {
        std::lock_guard<std::mutex> lock(servers_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [server_id, server] : servers_) {
            // 간단한 헬스체크 시뮬레이션
            bool was_healthy = server->is_healthy.load();
            
            // 일정 확률로 서버 상태 변경 (실제로는 ping/health check API 호출)
            if (std::rand() % 100 < 2) { // 2% 확률로 상태 변경
                server->is_healthy.store(!was_healthy);
            }
            
            server->last_health_check = now;
            
            // 오래된 업데이트의 서버는 unhealthy로 표시
            if (now - server->last_update > std::chrono::minutes(5)) {
                server->is_healthy.store(false);
            }
        }
    }

    void ProcessScalingRecommendations(const std::vector<ScalingRecommendation>& recommendations) {
        // 스케일링 권장사항 처리 (로그 출력 또는 자동화 시스템으로 전달)
        for (const auto& rec : recommendations) {
            if (rec.action != "maintain") {
                // 실제 구현에서는 클라우드 제공업체 API 호출하여 서버 추가/제거
                // 여기서는 시뮬레이션만 수행
            }
        }
    }
};

} // namespace mmorpg::network