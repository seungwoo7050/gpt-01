# Phase 96: Inventory UI

## Overview
This phase implements a comprehensive inventory management UI system for the MMORPG client. It includes inventory windows, equipment management, bank storage, and vendor interactions.

## Key Components

### 1. ItemSlot System (SEQUENCE: 2412-2421)
- **Base slot functionality**: Display items with icons and quantities
- **Quality borders**: Visual indication of item rarity
- **Drag and drop**: Full support for item movement
- **Cooldown overlay**: For consumable items
- **Tooltips**: Detailed item information on hover

### 2. Inventory Window (SEQUENCE: 2422-2435)
- **5 bag tabs**: Each with 6 slots (30 total)
- **Currency display**: Gold, silver, copper
- **Sort button**: Auto-organize items
- **Tab switching**: Easy navigation between bags
- **Drag callbacks**: Server synchronization support

### 3. Equipment Window (SEQUENCE: 2436-2442)
- **Character model area**: Visual representation space
- **13 equipment slots**: Full gear layout
  - Head, Shoulders, Chest, Hands, Belt, Legs, Feet
  - Main Hand, Off Hand
  - 2 Rings, 2 Trinkets
- **Stats display**: Primary and secondary attributes
- **Visual layout**: Intuitive slot positioning

### 4. Bank Window (SEQUENCE: 2443-2447)
- **8 bank tabs**: 98 slots per tab (784 total)
- **Deposit all button**: Quick storage
- **Tab navigation**: Easy access to all storage
- **Large capacity**: Significantly more than inventory

### 5. Vendor Window (SEQUENCE: 2448-2452)
- **Vendor grid**: Display merchant items
- **Buyback tab**: Recover sold items
- **Repair all button**: Equipment maintenance
- **Sell area**: Drag and drop to sell
- **Stock tracking**: Limited availability items

### 6. Inventory UI Manager (SEQUENCE: 2453-2460)
- **Centralized control**: All inventory-related windows
- **Toggle functions**: Show/hide windows
- **Update routing**: Efficient data distribution
- **Callback management**: Event handling

## Technical Implementation

### Item Quality System
```cpp
enum class ItemQuality {
    POOR,       // Gray
    COMMON,     // White
    UNCOMMON,   // Green
    RARE,       // Blue
    EPIC,       // Purple
    LEGENDARY,  // Orange
    ARTIFACT    // Gold
};
```

### Drag and Drop Flow
1. Mouse down on item slot → StartDrag()
2. Create drag visual with alpha transparency
3. Track mouse movement
4. Mouse up on target slot → EndDrag()
5. Validate move and update server

### Efficient Slot Management
- Reuse slot objects
- Only update visible slots
- Hide/show on tab switch
- Minimal re-rendering

## User Experience Features

### Visual Feedback
- Quality-based border colors
- Quantity text for stackable items
- Cooldown visualization
- Hover highlights

### Convenience Features
- Auto-sort functionality
- Deposit/sell all options
- Right-click context menus
- Shift-click item splitting

### Layout Design
- Logical equipment slot positions
- Clear bag organization
- Prominent currency display
- Intuitive tab system

## Integration Points

### Server Communication
```cpp
// Item movement
SetOnItemMove([](int from, int to) {
    // Send move request to server
});

// Item usage
SetOnItemUse([](int slot) {
    // Send use request to server
});
```

### Inventory System
- Links with inventory::Inventory structure
- Updates from server inventory changes
- Validates actions before sending

## Usage Example
```cpp
// Initialize UI
InventoryUIManager::Instance().Initialize();

// Toggle inventory with 'I' key
InventoryUIManager::Instance().ToggleInventory();

// Update inventory from server data
InventoryUIManager::Instance().UpdateInventory(player_inventory);

// Show vendor window when interacting with NPC
InventoryUIManager::Instance().ShowVendor();
```

## Next Steps
Phase 97 will implement the chat interface system for player communication.