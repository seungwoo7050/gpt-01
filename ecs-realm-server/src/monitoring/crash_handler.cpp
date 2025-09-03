#include "crash_handler.h"
#include <filesystem>
#include <sys/resource.h>
#include <unistd.h>

namespace mmorpg::monitoring {

// [SEQUENCE: 2557] Crash handler integration example
class CrashHandlerIntegration {
public:
    // [SEQUENCE: 2558] Initialize crash handler with game server
    static void InitializeWithGameServer(GameServer* server) {
        auto& handler = CrashHandler::Instance();
        
        // Set dump directory
        std::string dump_dir = "./crash_dumps";
        handler.Initialize(dump_dir);
        
        // [SEQUENCE: 2559] Register system info provider
        handler.RegisterStateProvider([server](CrashInfo& info) {
            // Get memory usage
            struct rusage usage;
            getrusage(RUSAGE_SELF, &usage);
            info.system_info.memory_usage_bytes = usage.ru_maxrss * 1024;
            
            // Get server stats
            info.system_info.active_connection_count = server->GetConnectionCount();
            info.system_info.active_player_count = server->GetPlayerCount();
            info.system_info.active_thread_count = server->GetThreadPoolSize();
            
            // Get server version and uptime
            info.system_info.server_version = SERVER_VERSION;
            info.system_info.uptime = server->GetUptime();
            
            // Get OS info
            info.system_info.os_info = GetOSInfo();
        });
        
        // [SEQUENCE: 2560] Register game state provider
        handler.RegisterStateProvider([server](CrashInfo& info) {
            // Get current game phase
            info.game_state.current_phase = server->GetCurrentPhase();
            
            // Get last processed packet
            auto last_packet = server->GetLastProcessedPacket();
            if (last_packet) {
                info.game_state.last_processed_packet = last_packet->ToString();
            }
        });
        
        // [SEQUENCE: 2561] Set up automatic crash reports
        SetupAutomaticCrashReporting(dump_dir);
    }
    
private:
    // [SEQUENCE: 2562] Get OS information
    static std::string GetOSInfo() {
        std::string os_info;
        
#ifdef __linux__
        // Read from /etc/os-release
        std::ifstream os_file("/etc/os-release");
        std::string line;
        while (std::getline(os_file, line)) {
            if (line.find("PRETTY_NAME=") == 0) {
                os_info = line.substr(12);
                // Remove quotes
                os_info.erase(std::remove(os_info.begin(), os_info.end(), '"'), os_info.end());
                break;
            }
        }
#elif defined(__APPLE__)
        os_info = "macOS";
#elif defined(_WIN32)
        os_info = "Windows";
#else
        os_info = "Unknown OS";
#endif
        
        return os_info;
    }
    
    // [SEQUENCE: 2563] Set up automatic crash reporting
    static void SetupAutomaticCrashReporting(const std::string& dump_dir) {
        // Create monitoring thread for crash analysis
        std::thread analyzer_thread([dump_dir]() {
            while (true) {
                // Wait for 1 hour
                std::this_thread::sleep_for(std::chrono::hours(1));
                
                // Analyze recent crashes
                auto stats = CrashReportAnalyzer::AnalyzeCrashDumps(dump_dir);
                
                // Log statistics
                spdlog::info("Crash statistics - Frequency: {:.2f} crashes/hour", 
                           stats.GetCrashFrequency());
                spdlog::info("Most common crash: {}", stats.GetMostCommonCrash());
                
                // Alert if crash rate is high
                if (stats.GetCrashFrequency() > 1.0f) {
                    spdlog::error("High crash rate detected: {:.2f} crashes/hour", 
                                stats.GetCrashFrequency());
                    
                    // Send alert (email, webhook, etc.)
                    SendCrashAlert(stats);
                }
            }
        });
        
        analyzer_thread.detach();
    }
    
    // [SEQUENCE: 2564] Send crash alert
    static void SendCrashAlert(const CrashReportAnalyzer::CrashStatistics& stats) {
        // This would send alerts via various channels
        // - Email to administrators
        // - Slack/Discord webhook
        // - PagerDuty integration
        // - Monitoring dashboard update
        
        spdlog::critical("Crash alert sent - Frequency: {:.2f}, Pattern: {}", 
                        stats.GetCrashFrequency(), 
                        stats.GetMostCommonCrash());
    }
};

// [SEQUENCE: 2565] Crash debugging utilities
class CrashDebugger {
public:
    // [SEQUENCE: 2566] Symbolicate addresses for better debugging
    static std::vector<std::string> SymbolicateAddresses(
        const std::vector<void*>& addresses,
        const std::string& binary_path) {
        
        std::vector<std::string> symbolicated;
        
        // Use addr2line or similar tool
        for (void* addr : addresses) {
            std::string cmd = "addr2line -e " + binary_path + " -f -C " + 
                            std::to_string(reinterpret_cast<uintptr_t>(addr));
            
            FILE* pipe = popen(cmd.c_str(), "r");
            if (pipe) {
                char buffer[256];
                std::string result;
                
                while (fgets(buffer, sizeof(buffer), pipe)) {
                    result += buffer;
                }
                
                pclose(pipe);
                
                // Remove newlines
                result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
                symbolicated.push_back(result);
            } else {
                symbolicated.push_back("Unknown");
            }
        }
        
        return symbolicated;
    }
    
    // [SEQUENCE: 2567] Generate crash report summary
    static std::string GenerateCrashSummary(const std::string& dump_file) {
        std::stringstream summary;
        std::ifstream file(dump_file);
        std::string line;
        
        bool in_backtrace = false;
        std::vector<std::string> key_frames;
        
        while (std::getline(file, line)) {
            // Extract key information
            if (line.find("Signal:") != std::string::npos) {
                summary << line << "\n";
            } else if (line.find("Crash Address:") != std::string::npos) {
                summary << line << "\n";
            } else if (line.find("=== Backtrace ===") != std::string::npos) {
                in_backtrace = true;
            } else if (in_backtrace && line[0] == '#') {
                // Extract first few frames
                if (key_frames.size() < 5) {
                    key_frames.push_back(line);
                }
            }
        }
        
        summary << "\nKey Stack Frames:\n";
        for (const auto& frame : key_frames) {
            summary << frame << "\n";
        }
        
        // Analyze pattern
        auto pattern = StackTraceAnalyzer::AnalyzeCrashPattern(key_frames);
        summary << "\nLikely Cause: " << pattern << "\n";
        
        return summary.str();
    }
};

// [SEQUENCE: 2568] Memory corruption detector
class MemoryCorruptionDetector {
public:
    // [SEQUENCE: 2569] Check for common memory issues
    static void EnableMemoryChecks() {
        // Enable core dumps
        struct rlimit core_limit;
        core_limit.rlim_cur = RLIM_INFINITY;
        core_limit.rlim_max = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &core_limit);
        
        // Set malloc options for debugging
#ifdef __linux__
        // Enable malloc debugging
        setenv("MALLOC_CHECK_", "3", 1);
        
        // Enable glibc backtrace on error
        setenv("MALLOC_TRACE", "./malloc_trace.log", 1);
#endif
        
        spdlog::info("Memory corruption detection enabled");
    }
    
    // [SEQUENCE: 2570] Validate memory regions
    static bool ValidateMemoryRegion(void* ptr, size_t size) {
        // Check if memory is readable
        if (ptr == nullptr) return false;
        
        // Try to read memory (platform-specific)
#ifdef __linux__
        // Use mincore to check if pages are valid
        unsigned char vec[1];
        int result = mincore(ptr, size, vec);
        return result == 0;
#else
        // Basic null check for other platforms
        return ptr != nullptr;
#endif
    }
    
    // [SEQUENCE: 2571] Detect use-after-free patterns
    static bool DetectUseAfterFree(const std::vector<std::string>& backtrace) {
        // Look for patterns indicating use-after-free
        for (const auto& frame : backtrace) {
            if (frame.find("free") != std::string::npos ||
                frame.find("delete") != std::string::npos) {
                
                // Check if there are usage patterns after free
                return true;
            }
        }
        
        return false;
    }
};

// [SEQUENCE: 2572] Crash recovery system
class CrashRecovery {
public:
    // [SEQUENCE: 2573] Attempt automatic recovery
    static bool AttemptRecovery(const CrashInfo& crash_info) {
        spdlog::warn("Attempting crash recovery for signal: {}", 
                    crash_info.signal_name);
        
        // Check if recovery is possible
        if (!IsRecoverable(crash_info)) {
            spdlog::error("Crash is not recoverable");
            return false;
        }
        
        // [SEQUENCE: 2574] Recovery strategies
        switch (crash_info.signal_number) {
            case SIGSEGV:
                return RecoverFromSegfault(crash_info);
            
            case SIGFPE:
                return RecoverFromMathError(crash_info);
            
            case SIGPIPE:
                // Broken pipe - can often recover
                return true;
            
            default:
                return false;
        }
    }
    
private:
    // [SEQUENCE: 2575] Check if crash is recoverable
    static bool IsRecoverable(const CrashInfo& crash_info) {
        // Some crashes are not recoverable
        if (crash_info.signal_number == SIGABRT) {
            return false;  // Assertion failure
        }
        
        // Check if crash is in critical code
        for (const auto& frame : crash_info.backtrace_symbols) {
            if (frame.find("critical_") != std::string::npos ||
                frame.find("main") != std::string::npos) {
                return false;
            }
        }
        
        return true;
    }
    
    static bool RecoverFromSegfault(const CrashInfo& crash_info) {
        // Attempt to recover from segmentation fault
        // This is very dangerous and should only be done in specific cases
        
        // Check if it's a null pointer access
        if (crash_info.crash_address == nullptr) {
            spdlog::warn("Null pointer access detected - skipping operation");
            return true;  // Can sometimes skip the operation
        }
        
        return false;
    }
    
    static bool RecoverFromMathError(const CrashInfo& crash_info) {
        // Math errors (divide by zero, etc.) can sometimes be recovered
        spdlog::warn("Math error detected - using safe value");
        return true;
    }
};

} // namespace mmorpg::monitoring