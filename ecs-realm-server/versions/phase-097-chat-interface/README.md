# Phase 97: Chat Interface

## Overview
This phase implements a comprehensive chat interface system for player communication in the MMORPG. It includes multi-channel support, tabbed chat windows, combat logging, and full text input functionality.

## Key Components

### 1. Chat Message System (SEQUENCE: 2462-2467)
- **Message data structure**: Channel, sender, text, timestamp, formatting
- **Message formatting**: Timestamps, channel prefixes, sender info
- **GM support**: Special [GM] tag for game masters
- **Title display**: Guild ranks and achievement titles
- **Height calculation**: For text wrapping support

### 2. Chat Window (SEQUENCE: 2468-2478)
- **Scrollable message area**: 1000 message history
- **Channel filtering**: Show/hide specific channels
- **Mouse wheel support**: Smooth scrolling
- **Auto-scroll**: Follows new messages when at bottom
- **Channel selector**: Quick channel switching
- **Command processing**: Slash commands support

### 3. Text Input System (SEQUENCE: 2479-2480)
- **Full text editing**: Insert, delete, cursor movement
- **Keyboard navigation**: Arrow keys, Home, End
- **Blinking cursor**: Visual feedback
- **Enter to send**: Standard submission
- **Focus management**: Click to activate

### 4. Tab System (SEQUENCE: 2481-2484)
- **Multiple chat tabs**: Organize channels by purpose
- **Default tabs**:
  - General: General, Trade, LocalDefense
  - Combat: Combat logs, Loot, Experience
  - Guild: Guild chat and officer channel
- **Tab switching**: Visual highlighting
- **Channel routing**: Messages go to appropriate tabs

### 5. Combat Log (SEQUENCE: 2485-2487)
- **Dedicated combat window**: Separate from chat
- **Event types**: Damage, healing, buffs/debuffs
- **Color coding**:
  - Damage: Red (1.0, 0.5, 0.5)
  - Healing: Green (0.5, 1.0, 0.5)
  - Buffs: Blue (0.5, 0.5, 1.0)
- **Filter buttons**: Toggle event types
- **Formatted messages**: Clear combat information

### 6. Channel Colors (SEQUENCE: 2491-2492)
```cpp
Say:     White   (1.0, 1.0, 1.0)
Yell:    Red     (1.0, 0.4, 0.4)
Party:   Blue    (0.4, 0.7, 1.0)
Guild:   Green   (0.4, 1.0, 0.4)
Raid:    Orange  (1.0, 0.5, 0.0)
Trade:   Peach   (1.0, 0.6, 0.4)
Whisper: Pink    (1.0, 0.5, 1.0)
System:  Yellow  (1.0, 1.0, 0.0)
```

### 7. Chat Commands
- `/say` or `/s` - Local chat
- `/party` or `/p` - Party chat
- `/guild` or `/g` - Guild chat
- `/whisper <player>` or `/w` - Private message
- `/help` - Show available commands

## Technical Implementation

### Efficient Rendering
- Only render visible messages
- Calculate message positions based on scroll
- Cache message heights
- Limit rendering to viewport + buffer

### Memory Management
- Deque for message storage
- 1000 message limit
- Automatic old message removal
- Efficient string handling

### Input Processing
```cpp
// Keyboard event handling
switch (key) {
    case 8:   // Backspace
    case 127: // Delete
    case 13:  // Enter
    case 37:  // Left arrow
    case 39:  // Right arrow
    case 36:  // Home
    case 35:  // End
}
```

### Message Flow
1. User types message
2. Press Enter → OnChatSubmit()
3. Check for commands (/)
4. Send via callback or process command
5. Server broadcasts message
6. Receive and display in appropriate tabs

## Chat UI Manager (SEQUENCE: 2488-2492)

### Responsibilities
- Initialize all chat windows
- Route messages to tabs
- Manage combat log visibility
- Handle callbacks for server communication

### Integration Example
```cpp
// Initialize chat system
ChatUIManager::Instance().Initialize();

// Set message sending callback
ChatUIManager::Instance().SetOnChatMessage(
    [network](const std::string& msg, ChatChannel channel) {
        network->SendChatMessage(msg, channel);
    }
);

// Receive message from server
void OnChatPacketReceived(const ChatPacket& packet) {
    ChatUIManager::Instance().AddChatMessage(
        packet.sender_name,
        packet.message,
        packet.channel
    );
}

// Add combat event
ChatUIManager::Instance().AddCombatEvent(
    "damage",           // Event type
    "PlayerName",       // Source
    "MonsterName",      // Target
    150,                // Amount
    "Fireball"          // Ability name
);
```

## User Experience Features

### Visual Design
- Semi-transparent background (0.7 alpha)
- Channel-specific colors
- Clear timestamp format
- Readable font size (14pt)

### Interaction
- Click channel button to switch
- Mouse wheel scrolling
- Tab clicking for organization
- Drag to resize (prepared)

### Convenience
- Auto-scroll on new messages
- Message filtering by channel
- Combat log toggle
- Command shortcuts

## Next Steps
Phase 98 will implement the map and minimap system for navigation interface.