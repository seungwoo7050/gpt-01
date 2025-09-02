#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <thread>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <cxxabi.h>
#include <fstream>
#include <spdlog/spdlog.h>

namespace mmorpg::monitoring {

// [SEQUENCE: MVP16-1] Crash handler system for debugging and diagnostics
// 크래시 핸들러 - 서버 크래시 시 덤프 생성 및 분석

// [SEQUENCE: MVP16-2] Crash information structure
struct CrashInfo {
    std::chrono::system_clock::time_point timestamp;
    int signal_number;                      // 시그널 번호 (SIGSEGV, SIGABRT 등)
    std::string signal_name;                // 시그널 이름
    void* crash_address;                    // 크래시 발생 주소
    std::vector<void*> backtrace_addresses; // 콜스택 주소
    std::vector<std::string> backtrace_symbols; // 심볼 정보
    std::thread::id thread_id;              // 크래시 발생 스레드
    
    // [SEQUENCE: MVP16-3] System information at crash time
    struct SystemInfo {
        size_t memory_usage_bytes;          // 메모리 사용량
        size_t peak_memory_bytes;           // 최대 메모리 사용량
        int active_thread_count;            // 활성 스레드 수
        int active_connection_count;        // 활성 연결 수
        int active_player_count;            // 접속 플레이어 수
        std::string server_version;         // 서버 버전
        std::string os_info;                // OS 정보
        std::chrono::milliseconds uptime;   // 서버 가동 시간
    } system_info;
    
    // [SEQUENCE: MVP16-4] Game state at crash time
    struct GameState {
        std::vector<std::string> recent_commands;  // 최근 실행된 명령어
        std::vector<std::string> recent_errors;    // 최근 에러 로그
        std::string current_phase;                 // 현재 게임 단계
        std::string last_processed_packet;         // 마지막 처리 패킷
    } game_state;
};

// [SEQUENCE: MVP16-5] Crash dump writer
class CrashDumpWriter {
public:
    CrashDumpWriter(const std::string& dump_directory) 
        : dump_directory_(dump_directory) {
        
        // [SEQUENCE: MVP16-6] Create dump directory if not exists
        std::filesystem::create_directories(dump_directory_);
    }
    
    // [SEQUENCE: MVP16-7] Write crash dump to file
    void WriteDump(const CrashInfo& crash_info) {
        auto timestamp = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        
        // Generate filename with timestamp
        char filename[256];
        std::strftime(filename, sizeof(filename), 
                     "crash_dump_%Y%m%d_%H%M%S.txt", 
                     std::localtime(&time_t));
        
        std::string filepath = dump_directory_ + "/" + filename;
        std::ofstream dump_file(filepath);
        
        if (!dump_file.is_open()) {
            spdlog::error("Failed to create crash dump file: {}", filepath);
            return;
        }
        
        // [SEQUENCE: MVP16-8] Write crash header
        dump_file << "=== MMORPG Server Crash Dump ===" << std::endl;
        dump_file << "Timestamp: " << FormatTimestamp(crash_info.timestamp) << std::endl;
        dump_file << "Server Version: " << crash_info.system_info.server_version << std::endl;
        dump_file << "OS: " << crash_info.system_info.os_info << std::endl;
        dump_file << "Uptime: " << crash_info.system_info.uptime.count() << " ms" << std::endl;
        dump_file << std::endl;
        
        // [SEQUENCE: MVP16-9] Write signal information
        dump_file << "=== Crash Information ===" << std::endl;
        dump_file << "Signal: " << crash_info.signal_name 
                  << " (" << crash_info.signal_number << ")" << std::endl;
        dump_file << "Crash Address: " << crash_info.crash_address << std::endl;
        dump_file << "Thread ID: " << crash_info.thread_id << std::endl;
        dump_file << std::endl;
        
        // [SEQUENCE: MVP16-10] Write backtrace
        dump_file << "=== Backtrace ===" << std::endl;
        for (size_t i = 0; i < crash_info.backtrace_symbols.size(); ++i) {
            dump_file << "#" << i << " " << crash_info.backtrace_symbols[i] << std::endl;
        }
        dump_file << std::endl;
        
        // [SEQUENCE: MVP16-11] Write system state
        dump_file << "=== System State ===" << std::endl;
        dump_file << "Memory Usage: " << FormatBytes(crash_info.system_info.memory_usage_bytes) << std::endl;
        dump_file << "Peak Memory: " << FormatBytes(crash_info.system_info.peak_memory_bytes) << std::endl;
        dump_file << "Active Threads: " << crash_info.system_info.active_thread_count << std::endl;
        dump_file << "Active Connections: " << crash_info.system_info.active_connection_count << std::endl;
        dump_file << "Active Players: " << crash_info.system_info.active_player_count << std::endl;
        dump_file << std::endl;
        
        // [SEQUENCE: MVP16-12] Write game state
        dump_file << "=== Game State ===" << std::endl;
        dump_file << "Current Phase: " << crash_info.game_state.current_phase << std::endl;
        dump_file << "Last Packet: " << crash_info.game_state.last_processed_packet << std::endl;
        
        dump_file << "\nRecent Commands:" << std::endl;
        for (const auto& cmd : crash_info.game_state.recent_commands) {
            dump_file << "  - " << cmd << std::endl;
        }
        
        dump_file << "\nRecent Errors:" << std::endl;
        for (const auto& err : crash_info.game_state.recent_errors) {
            dump_file << "  - " << err << std::endl;
        }
        
        dump_file.close();
        
        // [SEQUENCE: MVP16-13] Also write minidump for analysis tools
        WriteMinidump(crash_info, filepath + ".dmp");
        
        spdlog::critical("Crash dump written to: {}", filepath);
    }
    
private:
    std::string dump_directory_;
    
    std::string FormatTimestamp(const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        return buffer;
    }
    
    std::string FormatBytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB"};
        int unit_index = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024 && unit_index < 3) {
            size /= 1024;
            unit_index++;
        }
        
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
        return buffer;
    }
    
    void WriteMinidump(const CrashInfo& crash_info, const std::string& filepath) {
        // Platform-specific minidump generation
        // Linux: Use google-breakpad or custom core dump
        // Windows: Use MiniDumpWriteDump
        // This is a placeholder for the actual implementation
    }
};

// [SEQUENCE: MVP16-14] Stack trace analyzer
class StackTraceAnalyzer {
public:
    // [SEQUENCE: MVP16-15] Capture current stack trace
    static std::vector<std::string> CaptureStackTrace(int skip_frames = 1) {
        std::vector<std::string> stack_trace;
        
        void* buffer[128];
        int frame_count = backtrace(buffer, 128);
        
        if (frame_count <= skip_frames) {
            return stack_trace;
        }
        
        char** symbols = backtrace_symbols(buffer + skip_frames, 
                                          frame_count - skip_frames);
        
        if (symbols) {
            for (int i = 0; i < frame_count - skip_frames; ++i) {
                stack_trace.push_back(DemangleSymbol(symbols[i]));
            }
            free(symbols);
        }
        
        return stack_trace;
    }
    
    // [SEQUENCE: MVP16-16] Analyze crash pattern
    static std::string AnalyzeCrashPattern(const std::vector<std::string>& backtrace) {
        // Common crash patterns
        if (ContainsPattern(backtrace, "malloc") || ContainsPattern(backtrace, "free")) {
            return "Memory corruption or double-free detected";
        }
        
        if (ContainsPattern(backtrace, "std::vector") && ContainsPattern(backtrace, "at")) {
            return "Vector out-of-bounds access";
        }
        
        if (ContainsPattern(backtrace, "null") || ContainsPattern(backtrace, "0x0")) {
            return "Null pointer dereference";
        }
        
        if (ContainsPattern(backtrace, "stack_overflow")) {
            return "Stack overflow detected";
        }
        
        if (ContainsPattern(backtrace, "pure virtual")) {
            return "Pure virtual function call";
        }
        
        return "Unknown crash pattern";
    }
    
private:
    // [SEQUENCE: MVP16-17] Demangle C++ symbols
    static std::string DemangleSymbol(const char* symbol) {
        std::string result(symbol);
        
        // Extract mangled name from symbol
        // Format: module(function+offset) [address]
        size_t start = result.find('(');
        size_t end = result.find('+', start);
        
        if (start != std::string::npos && end != std::string::npos) {
            std::string mangled = result.substr(start + 1, end - start - 1);
            
            int status;
            char* demangled = abi::__cxa_demangle(mangled.c_str(), nullptr, nullptr, &status);
            
            if (status == 0 && demangled) {
                result.replace(start + 1, end - start - 1, demangled);
                free(demangled);
            }
        }
        
        return result;
    }
    
    static bool ContainsPattern(const std::vector<std::string>& backtrace, 
                                const std::string& pattern) {
        for (const auto& frame : backtrace) {
            if (frame.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

// [SEQUENCE: MVP16-18] Crash handler singleton
class CrashHandler {
public:
    static CrashHandler& Instance() {
        static CrashHandler instance;
        return instance;
    }
    
    // [SEQUENCE: MVP16-19] Initialize crash handler
    void Initialize(const std::string& dump_directory) {
        dump_writer_ = std::make_unique<CrashDumpWriter>(dump_directory);
        
        // Install signal handlers
        InstallSignalHandlers();
        
        // Set up unhandled exception handler
        std::set_terminate([]() {
            OnUnhandledException();
        });
        
        spdlog::info("Crash handler initialized with dump directory: {}", dump_directory);
    }
    
    // [SEQUENCE: MVP16-20] Register state provider
    void RegisterStateProvider(std::function<void(CrashInfo&)> provider) {
        state_providers_.push_back(provider);
    }
    
    // [SEQUENCE: MVP16-21] Manual crash dump
    void GenerateManualDump(const std::string& reason) {
        CrashInfo crash_info;
        crash_info.timestamp = std::chrono::system_clock::now();
        crash_info.signal_number = 0;
        crash_info.signal_name = "Manual Dump";
        crash_info.crash_address = nullptr;
        crash_info.thread_id = std::this_thread::get_id();
        
        // Capture stack trace
        auto symbols = StackTraceAnalyzer::CaptureStackTrace(2);
        crash_info.backtrace_symbols = symbols;
        
        // Collect state
        CollectCrashState(crash_info);
        
        // Add manual dump reason
        crash_info.game_state.current_phase = "Manual Dump: " + reason;
        
        // Write dump
        if (dump_writer_) {
            dump_writer_->WriteDump(crash_info);
        }
    }
    
    // [SEQUENCE: MVP16-22] Add recent command for crash context
    void AddRecentCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(command_mutex_);
        recent_commands_.push_back(command);
        
        // Keep only last 20 commands
        if (recent_commands_.size() > 20) {
            recent_commands_.pop_front();
        }
    }
    
    void AddRecentError(const std::string& error) {
        std::lock_guard<std::mutex> lock(error_mutex_);
        recent_errors_.push_back(error);
        
        // Keep only last 50 errors
        if (recent_errors_.size() > 50) {
            recent_errors_.pop_front();
        }
    }
    
private:
    CrashHandler() = default;
    
    std::unique_ptr<CrashDumpWriter> dump_writer_;
    std::vector<std::function<void(CrashInfo&)>> state_providers_;
    
    // Recent activity tracking
    std::deque<std::string> recent_commands_;
    std::deque<std::string> recent_errors_;
    std::mutex command_mutex_;
    std::mutex error_mutex_;
    
    // [SEQUENCE: MVP16-23] Install signal handlers
    void InstallSignalHandlers() {
        struct sigaction sa;
        sa.sa_sigaction = SignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        
        // Install handlers for various signals
        sigaction(SIGSEGV, &sa, nullptr);  // Segmentation fault
        sigaction(SIGABRT, &sa, nullptr);  // Abort
        sigaction(SIGFPE, &sa, nullptr);   // Floating point exception
        sigaction(SIGILL, &sa, nullptr);   // Illegal instruction
        sigaction(SIGBUS, &sa, nullptr);   // Bus error
    }
    
    // [SEQUENCE: MVP16-24] Signal handler
    static void SignalHandler(int signal, siginfo_t* info, void* context) {
        // Prevent recursive crashes
        static std::atomic<bool> handling_crash{false};
        if (handling_crash.exchange(true)) {
            _exit(1);
        }
        
        CrashInfo crash_info;
        crash_info.timestamp = std::chrono::system_clock::now();
        crash_info.signal_number = signal;
        crash_info.signal_name = GetSignalName(signal);
        crash_info.crash_address = info->si_addr;
        crash_info.thread_id = std::this_thread::get_id();
        
        // Capture backtrace
        void* buffer[128];
        int frame_count = backtrace(buffer, 128);
        
        for (int i = 0; i < frame_count; ++i) {
            crash_info.backtrace_addresses.push_back(buffer[i]);
        }
        
        // Get symbols
        char** symbols = backtrace_symbols(buffer, frame_count);
        if (symbols) {
            for (int i = 0; i < frame_count; ++i) {
                crash_info.backtrace_symbols.push_back(
                    StackTraceAnalyzer::DemangleSymbol(symbols[i])
                );
            }
            free(symbols);
        }
        
        // Collect crash state
        Instance().CollectCrashState(crash_info);
        
        // Write crash dump
        if (Instance().dump_writer_) {
            Instance().dump_writer_->WriteDump(crash_info);
        }
        
        // Re-raise signal for core dump
        signal(signal, SIG_DFL);
        raise(signal);
    }
    
    // [SEQUENCE: MVP16-25] Unhandled exception handler
    static void OnUnhandledException() {
        try {
            // Re-throw to get exception info
            if (std::current_exception()) {
                throw;
            }
        } catch (const std::exception& e) {
            spdlog::critical("Unhandled exception: {}", e.what());
            Instance().GenerateManualDump(std::string("Unhandled exception: ") + e.what());
        } catch (...) {
            spdlog::critical("Unknown unhandled exception");
            Instance().GenerateManualDump("Unknown unhandled exception");
        }
        
        std::abort();
    }
    
    // [SEQUENCE: MVP16-26] Collect crash state from providers
    void CollectCrashState(CrashInfo& crash_info) {
        // Copy recent activity
        {
            std::lock_guard<std::mutex> lock(command_mutex_);
            crash_info.game_state.recent_commands.assign(
                recent_commands_.begin(), recent_commands_.end()
            );
        }
        
        {
            std::lock_guard<std::mutex> lock(error_mutex_);
            crash_info.game_state.recent_errors.assign(
                recent_errors_.begin(), recent_errors_.end()
            );
        }
        
        // Call registered state providers
        for (const auto& provider : state_providers_) {
            try {
                provider(crash_info);
            } catch (...) {
                // Ignore exceptions in crash handler
            }
        }
    }
    
    static std::string GetSignalName(int signal) {
        switch (signal) {
            case SIGSEGV: return "SIGSEGV (Segmentation fault)";
            case SIGABRT: return "SIGABRT (Abort)";
            case SIGFPE:  return "SIGFPE (Floating point exception)";
            case SIGILL:  return "SIGILL (Illegal instruction)";
            case SIGBUS:  return "SIGBUS (Bus error)";
            default:      return "Unknown signal " + std::to_string(signal);
        }
    }
};

// [SEQUENCE: MVP16-27] Crash report analyzer for post-mortem analysis
class CrashReportAnalyzer {
public:
    struct CrashStatistics {
        std::unordered_map<std::string, int> crash_by_signal;
        std::unordered_map<std::string, int> crash_by_pattern;
        std::unordered_map<std::string, int> crash_by_function;
        std::vector<std::chrono::system_clock::time_point> crash_times; 
        
        // [SEQUENCE: MVP16-28] Calculate crash frequency
        float GetCrashFrequency() const {
            if (crash_times.size() < 2) return 0;
            
            auto duration = crash_times.back() - crash_times.front();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
            
            return hours > 0 ? static_cast<float>(crash_times.size()) / hours : 0;
        }
        
        // [SEQUENCE: MVP16-29] Get most common crash
        std::string GetMostCommonCrash() const {
            std::string most_common;
            int max_count = 0;
            
            for (const auto& [pattern, count] : crash_by_pattern) {
                if (count > max_count) {
                    max_count = count;
                    most_common = pattern;
                }
            }
            
            return most_common;
        }
    };
    
    // [SEQUENCE: MVP16-30] Analyze crash dump files
    static CrashStatistics AnalyzeCrashDumps(const std::string& dump_directory) {
        CrashStatistics stats;
        
        // Scan directory for crash dumps
        for (const auto& entry : std::filesystem::directory_iterator(dump_directory)) {
            if (entry.path().extension() == ".txt") {
                AnalyzeSingleDump(entry.path().string(), stats);
            }
        }
        
        return stats;
    }
    
private:
    static void AnalyzeSingleDump(const std::string& filepath, CrashStatistics& stats) {
        // Parse crash dump file and extract statistics
        // This is a simplified implementation
        std::ifstream file(filepath);
        std::string line;
        
        while (std::getline(file, line)) {
            // Extract signal type
            if (line.find("Signal:") != std::string::npos) {
                auto signal = ExtractValue(line, "Signal:");
                stats.crash_by_signal[signal]++;
            }
            
            // Extract crash pattern
            if (line.find("Pattern:") != std::string::npos) {
                auto pattern = ExtractValue(line, "Pattern:");
                stats.crash_by_pattern[pattern]++;
            }
        }
    }
    
    static std::string ExtractValue(const std::string& line, const std::string& key) {
        auto pos = line.find(key);
        if (pos != std::string::npos) {
            return line.substr(pos + key.length() + 1);
        }
        return "";
    }
};

} // namespace mmorpg::monitoring
