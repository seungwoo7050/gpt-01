## MVP 12: UI System (Phase 95-98)

This MVP implements the foundational elements of the player's User Interface (UI). A comprehensive UI is critical for conveying information and enabling player interaction with the game world. This phase covers the foundational UI framework, the Heads-Up Display (HUD), inventory management, the chat interface, and the map system.

### Build System (`CMakeLists.txt`)

The UI system is composed of several header-only files. While they don't need to be compiled into a library, they must be included in the project's search path.

```cmake
# [SEQUENCE: MVP12-1] Add UI header directory to include path (reconstructed)
target_include_directories(mmorpg_game PRIVATE
    # ... other include directories
    src/ui
)
```

### Phase 95-98: UI Framework & Core Elements

This foundational phase establishes the bedrock of the entire UI system, defining the base classes, interfaces, and management singletons that all other UI components depend on.

**`src/ui/ui_framework.h`**
```cpp
#pragma once
// [SEQUENCE: MVP12-2] UI framework for client-side interface
#include <vector>
#include <memory>
#include <string>
#include <functional>

namespace mmorpg::ui {

// [SEQUENCE: MVP12-3] Basic UI types and enums
struct Color;
struct Vector2;
struct Rect;
enum class AnchorType;
enum class Visibility;
enum class UIState;

// [SEQUENCE: MVP12-4] Base UI element class
class UIElement {
public:
    UIElement(const std::string& name); // [SEQUENCE: MVP12-5]
    virtual ~UIElement() = default;
    
    virtual void Update(float delta_time); // [SEQUENCE: MVP12-6]
    virtual void Render(); // [SEQUENCE: MVP12-7]
    
    virtual bool HandleMouseMove(float x, float y); // [SEQUENCE: MVP12-8]
    virtual bool HandleMouseButton(int button, bool pressed, float x, float y); // [SEQUENCE: MVP12-9]
    
    void AddChild(std::shared_ptr<UIElement> child); // [SEQUENCE: MVP12-10]
    void SetPosition(const Vector2& position); // [SEQUENCE: MVP12-11]
    void SetSize(const Vector2& size); // [SEQUENCE: MVP12-12]
    void SetVisibility(Visibility visibility); // [SEQUENCE: MVP12-13]
};

// [SEQUENCE: MVP12-14] UI Panel - Container element
class UIPanel : public UIElement {
public:
    UIPanel(const std::string& name); // [SEQUENCE: MVP12-15]
    void SetBackgroundColor(const Color& color); // [SEQUENCE: MVP12-16]
protected:
    void OnRender() override; // [SEQUENCE: MVP12-17]
};

// [SEQUENCE: MVP12-18] UI Button
class UIButton : public UIElement {
public:
    UIButton(const std::string& name); // [SEQUENCE: MVP12-19]
    void SetText(const std::string& text); // [SEQUENCE: MVP12-20]
    void SetOnClick(std::function<void()> callback); // [SEQUENCE: MVP12-21]
protected:
    void OnRender() override; // [SEQUENCE: MVP12-22]
    void OnClick() override; // [SEQUENCE: MVP12-23]
};

// [SEQUENCE: MVP12-24] UI Manager - Manages all UI elements
class UIManager {
public:
    static UIManager& Instance(); // [SEQUENCE: MVP12-25]
    void SetRoot(std::shared_ptr<UIElement> root); // [SEQUENCE: MVP12-26]
    void Update(float delta_time); // [SEQUENCE: MVP12-27]
    void Render(); // [SEQUENCE: MVP12-28]
};
}
```

**`src/ui/hud_elements.h`**
```cpp
#pragma once
#include "ui_framework.h"

namespace mmorpg::ui {

// [SEQUENCE: MVP12-29] HUD (Heads-Up Display) elements
class HealthBar;
class ActionBar;
class HUDManager;

// [SEQUENCE: MVP12-30] Health bar widget
class HealthBar : public UIElement {
public:
    HealthBar(const std::string& name); // [SEQUENCE: MVP12-31]
    void SetHealth(float current, float max); // [SEQUENCE: MVP12-32]
};

// [SEQUENCE: MVP12-33] Action bar for abilities
class ActionBar : public UIElement {
public:
    ActionBar(const std::string& name, int slot_count = 12); // [SEQUENCE: MVP12-34]
    void SetAbility(int slot_index, uint32_t ability_id, uint32_t icon_id); // [SEQUENCE: MVP12-35]
};

// [SEQUENCE: MVP12-36] HUD manager
class HUDManager {
public:
    static HUDManager& Instance(); // [SEQUENCE: MVP12-37]
    void Initialize(); // [SEQUENCE: MVP12-38]
    void UpdatePlayerHealth(float current, float max); // [SEQUENCE: MVP12-39]
};
}
```

### Phase 96: Inventory UI

This phase implements the UI for player inventory, character equipment, banking, and vendors.

**`src/ui/inventory_ui.h`**
```cpp
#pragma once
#include "ui_framework.h"
#include "../inventory/inventory_system.h"

namespace mmorpg::ui {

// [SEQUENCE: MVP12-40] Inventory UI system
class ItemSlot;
class InventoryWindow;
class InventoryUIManager;

// [SEQUENCE: MVP12-41] Item slot for inventory grid
class ItemSlot : public UIButton {
public:
    ItemSlot(const std::string& name); // [SEQUENCE: MVP12-42]
    void SetItem(const inventory::Item* item); // [SEQUENCE: MVP12-43]
    void ClearSlot(); // [SEQUENCE: MVP12-44]
};

// [SEQUENCE: MVP12-45] Inventory window UI
class InventoryWindow : public UIWindow {
public:
    InventoryWindow(const std::string& name); // [SEQUENCE: MVP12-46]
    void UpdateInventory(const inventory::Inventory& inventory); // [SEQUENCE: MVP12-47]
};

// [SEQUENCE: MVP12-48] Inventory UI manager
class InventoryUIManager {
public:
    static InventoryUIManager& Instance(); // [SEQUENCE: MVP12-49]
    void Initialize(); // [SEQUENCE: MVP12-50]
    void ToggleInventory(); // [SEQUENCE: MVP12-51]
};
}
```

### Phase 97: Chat Interface

A flexible chat system is implemented, featuring multiple channels, commands, and customizable tabs.

**`src/ui/chat_interface.h`**
```cpp
#pragma once
#include "ui_framework.h"
#include "../social/chat_system.h"

namespace mmorpg::ui {

// [SEQUENCE: MVP12-52] Chat interface system
class ChatWindow;
class ChatUIManager;

// [SEQUENCE: MVP12-53] Chat display window
class ChatWindow : public UIPanel {
public:
    ChatWindow(const std::string& name); // [SEQUENCE: MVP12-54]
    void AddMessage(const social::ChatMessage& data); // [SEQUENCE: MVP12-55]
};

// [SEQUENCE: MVP12-56] Chat UI manager
class ChatUIManager {
public:
    static ChatUIManager& Instance(); // [SEQUENCE: MVP12-57]
    void Initialize(); // [SEQUENCE: MVP12-58]
    void AddChatMessage(const std::string& sender, const std::string& message, social::ChatChannelType channel); // [SEQUENCE: MVP12-59]
};
}
```

### Phase 98: Map Interface

This phase implements the navigation UI, including a minimap and a world map.

**`src/ui/map_interface.h`**
```cpp
#pragma once
#include "ui_framework.h"

namespace mmorpg::ui {

// [SEQUENCE: MVP12-60] Map interface system
class Minimap;
class WorldMapWindow;
class MapUIManager;

// [SEQUENCE: MVP12-61] Minimap widget
class Minimap : public UIPanel {
public:
    Minimap(const std::string& name); // [SEQUENCE: MVP12-62]
    void UpdatePlayerPosition(float x, float y, float facing); // [SEQUENCE: MVP12-63]
};

// [SEQUENCE: MVP12-64] World map window
class WorldMapWindow : public UIWindow {
public:
    WorldMapWindow(const std::string& name); // [SEQUENCE: MVP12-65]
    void UpdatePlayerPosition(float world_x, float world_y); // [SEQUENCE: MVP12-66]
};

// [SEQUENCE: MVP12-67] Map UI manager
class MapUIManager {
public:
    static MapUIManager& Instance(); // [SEQUENCE: MVP12-68]
    void Initialize(); // [SEQUENCE: MVP12-69]
    void UpdatePlayerPosition(float x, float y, float facing); // [SEQUENCE: MVP12-70]
};
}
```

### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:**
    *   **UI 레이아웃 외부화:** UI 요소의 위치, 크기, 색상, 폰트 등 모든 시각적 정보는 코드에서 분리하여 외부 파일(XML, JSON 등)에서 로드해야 합니다. 이를 통해 UI/UX 디자이너가 개발자의 도움 없이 UI를 수정하고 반복 작업을 할 수 있습니다.
    *   **개인화 설정 저장:** 플레이어가 조정한 UI 설정(예: 창 위치, 액션바 잠금, 채팅 채널 필터)은 모두 데이터베이스에 저장하여, 다음 접속 시에도 동일한 환경을 제공해야 합니다.
*   **API 및 운영 도구:**
    *   UI 관련 문제를 디버깅하기 위해 GM 명령어가 필요합니다. 예를 들어, `/ui reload` (UI 스크립트 다시 로드), `/ui debug <element_name>` (특정 UI 요소의 경계와 상태 정보 표시) 등이 유용합니다.
*   **성능 및 확장성:**
    *   **서버-클라이언트 역할 분리:** 서버는 UI의 모양이 아닌 **상태 데이터**만 보내야 합니다. 예를 들어, 서버는 "플레이어의 체력이 80/100이다"라는 정보만 보내고, 이 정보를 바탕으로 체력 바를 어떻게 렌더링할지는 전적으로 클라이언트가 결정합니다. 이는 서버 부하를 줄이고 클라이언트의 반응성을 높이는 핵심 원칙입니다.
    *   **업데이트 최적화:** 서버는 매 프레임 모든 UI 상태를 보내는 대신, 변경된 데이터만 모아서 보내야 합니다(Delta Update). 예를 들어, 체력이 변경되었을 때만 체력 정보를 보내고, 위치가 그대로인 대상의 좌표는 보내지 않습니다.
*   **보안:**
    *   **서버 권위:** 클라이언트의 UI 상태를 절대 신뢰해서는 안 됩니다. 예를 들어, 클라이언트가 "보이지 않는 버튼을 눌렀다"거나 "잠긴 인벤토리 슬롯에서 아이템을 옮겼다"는 요청을 보내면, 서버는 해당 UI가 실제로 상호작용 가능한 상태인지 서버 측 데이터로 검증해야 합니다.
    *   **정보 은닉:** 은신(Stealth) 상태인 적 플레이어의 정보나, 아직 공개되지 않은 퀘스트 정보 등은 클라이언트의 UI 시스템에 아예 전송하지 않아야 데이터 마이닝이나 핵을 통한 정보 유출을 막을 수 있습니다.