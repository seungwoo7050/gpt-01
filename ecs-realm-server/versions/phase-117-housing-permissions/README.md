# Phase 117: Housing Permissions

## Overview
Implemented comprehensive housing permission system with access control, sharing mechanisms, group management, and visitor tracking for secure and flexible house sharing.

## Key Features

### 1. Permission Levels
Eight hierarchical permission levels:
- **No Access**: Denied entry
- **Visitor**: Basic access
- **Friend**: Extended privileges
- **Decorator**: Decoration management
- **Roommate**: Living privileges
- **Manager**: Administrative rights
- **Co-Owner**: Near-full control
- **Owner**: Complete control

### 2. Permission Flags
15 granular permission flags:
- House entry and furniture use
- Storage and crafting station access
- Decoration placement/removal
- Room modification
- Guest invitation
- Permission management
- Private room access
- Garden harvesting
- Pet feeding
- Mail collection
- Rent payment
- House sale rights

### 3. Access Control Features

#### Time Restrictions
- Set access windows
- Temporary permissions
- Scheduled access

#### Room Restrictions
- Limit access to specific rooms
- Private area protection
- Selective permissions

#### Ban System
- Player banning with reasons
- Ban history tracking
- Automatic permission revocation

#### Guest System
- Temporary visitor access
- Duration-based permissions
- Auto-expiration

### 4. Permission Groups
- Create named groups
- Batch permission assignment
- Member management
- Group templates

### 5. House Sharing System

#### Sharing Types
- Private (owner only)
- Friends only
- Guild members only
- Public access
- Ticketed entry

#### Showcase Mode
- House exhibitions
- Tagged searches
- Rating system
- Visitor tracking

#### Visitor Features
- Visit history
- Recent visitor list
- Visitor statistics
- Peak time tracking

### 6. Permission Templates
Pre-configured permission sets:
- Visitor template
- Friend template
- Roommate template
- Decorator template
- Manager template

## Technical Implementation

### Permission Validation
```cpp
bool CanPerformAction(player_id, action) {
    // Owner bypass
    if (player_id == owner_id) return true;
    
    // Ban check
    if (IsBanned(player_id)) return false;
    
    // Permission check
    auto perms = GetPermissions(player_id);
    return perms && perms->HasFlag(action);
}
```

### Group System
```cpp
class PermissionGroups {
    // Create groups for batch management
    uint32_t CreateGroup(name, creator_id);
    void AddMember(group_id, player_id);
    void SetGroupPermissions(group_id, permissions);
};
```

### Sharing Configuration
```cpp
struct SharingSettings {
    SharingType type;
    bool requires_approval;
    uint64_t visitor_fee;
    uint32_t max_visitors;
    
    // Showcase features
    bool showcase_mode;
    string title, description;
    vector<string> tags;
    
    // Ratings
    float average_rating;
    uint32_t total_ratings;
};
```

## Security Features

1. **Lockdown Mode**
   - Emergency access revocation
   - Permission caching
   - Quick restoration

2. **Validation System**
   - Level-based flag validation
   - Time restriction checks
   - Room access verification

3. **Audit Trail**
   - Permission grant history
   - Access attempt logging
   - Ban reason tracking

## Performance Optimizations

1. **Fast Lookups**
   - O(1) permission checks
   - Cached access results
   - Bitwise operations

2. **Batch Operations**
   - Group assignments
   - Template applications
   - Bulk revocations

3. **Memory Efficiency**
   - Shared permission sets
   - Lazy loading
   - Periodic cleanup

## Integration Points

### Housing System
- Room-based permissions
- House ownership validation
- Access point control

### Social System
- Friend list integration
- Guild member checks
- Block list respect

### Economic System
- Visitor fees
- Showcase revenue
- Rent collection

### UI System
- Permission management UI
- Visitor tracking display
- Rating interface

## Usage Example

```cpp
// Grant friend permissions
auto& manager = HousingPermissionManager::Instance();
auto* access = manager.GetHouseAccess(house_id);

PermissionSet friend_perms = PermissionTemplates::GetFriendTemplate();
access->GrantPermission(friend_id, friend_perms);

// Create decorator group
auto& groups = manager.GetGroupSystem();
uint32_t group_id = groups.CreateGroup("Decorators", owner_id);

// Add members and set permissions
groups.AddMember(group_id, decorator1_id);
groups.AddMember(group_id, decorator2_id);
groups.SetGroupPermissions(group_id, decorator_perms);

// Enable showcase mode
auto& sharing = manager.GetSharingSystem();
SharingSettings settings;
settings.type = SharingType::PUBLIC;
settings.showcase_mode = true;
settings.showcase_title = "Amazing Fantasy House";
sharing.UpdateSharingSettings(house_id, settings);
```

## Files Created
- src/housing/housing_permissions.h
- src/housing/housing_permissions.cpp

## Future Enhancements
1. Permission inheritance
2. Scheduled permission changes
3. Permission trading/selling
4. Cross-server permissions
5. Mobile app integration