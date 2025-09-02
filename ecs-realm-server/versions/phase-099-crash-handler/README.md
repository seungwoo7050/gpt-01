# Phase 99: Crash Dump Analysis

## Overview
This phase implements a comprehensive crash handling and analysis system for the MMORPG server. It automatically captures crash information, generates detailed dumps, analyzes patterns, and provides recovery mechanisms.

## Key Components

### 1. Crash Information Structure (SEQUENCE: 2528-2530)
- **Basic info**: Signal type, crash address, thread ID, timestamp
- **System state**: Memory usage, thread count, active connections
- **Game state**: Recent commands, errors, last packet processed
- **Full backtrace**: Complete call stack with demangled symbols

### 2. Crash Dump Writer (SEQUENCE: 2531-2539)
- **Structured text dumps**: Human-readable crash reports
- **Binary minidumps**: For debugger analysis
- **Automatic file naming**: Timestamp-based organization
- **Directory management**: Auto-creation and cleanup

### 3. Stack Trace Analyzer (SEQUENCE: 2540-2543)
- **Symbol demangling**: C++ name demangling for readability
- **Pattern recognition**: Automatic crash cause identification
- **Common patterns**:
  - Memory corruption (malloc/free issues)
  - Null pointer dereference
  - Vector out-of-bounds
  - Stack overflow
  - Pure virtual calls

### 4. Signal Handler System (SEQUENCE: 2544-2552)
- **Handled signals**:
  - SIGSEGV: Segmentation faults
  - SIGABRT: Assertion failures
  - SIGFPE: Math errors (divide by zero)
  - SIGILL: Illegal instructions
  - SIGBUS: Bus errors
- **Recursive crash prevention**: Atomic flag to prevent handler loops
- **Exception handling**: std::terminate handler for C++ exceptions

### 5. Crash Statistics (SEQUENCE: 2553-2556)
- **Frequency analysis**: Crashes per hour calculation
- **Pattern tracking**: Most common crash types
- **Function statistics**: Which functions crash most
- **Time-based analysis**: When crashes occur

### 6. Server Integration (SEQUENCE: 2557-2564)
```cpp
// Initialize crash handler
CrashHandler::Instance().Initialize("./crash_dumps");

// Register state providers
handler.RegisterStateProvider([server](CrashInfo& info) {
    info.system_info.active_players = server->GetPlayerCount();
    info.system_info.memory_usage = GetMemoryUsage();
});

// Track recent activity
handler.AddRecentCommand("PLAYER_LOGIN user123");
handler.AddRecentError("Socket timeout");
```

### 7. Memory Corruption Detection (SEQUENCE: 2568-2571)
- **Core dump enabling**: Unlimited core file size
- **Malloc debugging**: MALLOC_CHECK_ environment variable
- **Memory validation**: Check if pointers are valid
- **Use-after-free detection**: Pattern analysis in crashes

### 8. Crash Recovery (SEQUENCE: 2572-2575)
- **Recoverability check**: Determine if recovery is possible
- **Recovery strategies**:
  - SIGPIPE: Often recoverable (broken connection)
  - SIGSEGV: Sometimes recoverable (null checks)
  - SIGFPE: Usually recoverable (safe math values)
- **Critical code protection**: No recovery in main/critical functions

## Technical Implementation

### Signal Safety
- Only async-signal-safe functions in handlers
- No dynamic memory allocation in crash path
- Static buffers for crash data
- Atomic operations for state management

### Platform Support
- **Linux**: Full support with backtrace, addr2line
- **macOS**: Partial support (no addr2line)
- **Windows**: Requires MinGW or custom implementation

### Performance Impact
- Zero overhead during normal operation
- Crash handling adds ~10-50ms to shutdown
- Automatic analysis runs in separate thread
- Minimal memory footprint

## Crash Dump Format
```
=== MMORPG Server Crash Dump ===
Timestamp: 2024-01-20 15:30:45
Server Version: 1.0.0
OS: Ubuntu 22.04 LTS
Uptime: 3600000 ms

=== Crash Information ===
Signal: SIGSEGV (11)
Crash Address: 0x0
Thread ID: 140234567890

=== Backtrace ===
#0 mmorpg::Player::GetName() at player.cpp:45
#1 mmorpg::CombatSystem::ProcessAttack() at combat.cpp:123
#2 mmorpg::GameServer::Update() at server.cpp:789

=== System State ===
Memory Usage: 1.23 GB
Peak Memory: 1.45 GB
Active Threads: 12
Active Connections: 1523
Active Players: 1485

=== Game State ===
Current Phase: Combat Processing
Last Packet: SKILL_USE
Recent Commands:
  - MOVE_PLAYER 12345 100.5 200.3
  - CAST_SPELL 12345 678
Recent Errors:
  - Invalid target for spell 678
  - Network timeout for player 9999
```

## Automatic Alerts
- Crash frequency monitoring (hourly check)
- Alert if >1 crash per hour
- Notification channels:
  - Email to administrators
  - Slack/Discord webhooks
  - PagerDuty integration
  - Monitoring dashboard

## Usage Guide

### Manual Dump Generation
```cpp
// Generate dump for investigation
CrashHandler::Instance().GenerateManualDump("Investigating memory leak");
```

### Crash Analysis
```cpp
// Analyze crash patterns
auto stats = CrashReportAnalyzer::AnalyzeCrashDumps("./crash_dumps");
spdlog::info("Most common crash: {}", stats.GetMostCommonCrash());
spdlog::info("Crash rate: {} per hour", stats.GetCrashFrequency());
```

### Debugging Crashes
1. Check text dump for quick overview
2. Load minidump in gdb for detailed analysis
3. Use addr2line for source locations
4. Review recent commands/errors for context

## Best Practices
1. **Prevention**: Regular code reviews, static analysis
2. **Testing**: Stress tests, fuzzing, memory sanitizers
3. **Monitoring**: Track crash trends over time
4. **Response**: Fix high-frequency crashes first
5. **Documentation**: Document all crash patterns

## Next Steps
Phase 100 will implement A/B testing framework to complete MVP14 (Monitoring & Analytics).