#pragma once

#include <unordered_map>
#include <vector>
#include <deque>
#include <chrono>
#include <optional>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace mmorpg::game::economy {

// [SEQUENCE: 1913] Trading post system for commodity exchange
// p˜Œ Ü¤\<\ ¬Ì  Áˆ PX

// [SEQUENCE: 1914] Order type
enum class OrderType {
    BUY,                // Buy order
    SELL               // Sell order
};

// [SEQUENCE: 1915] Order status
enum class OrderStatus {
    ACTIVE,             // Open order
    FILLED,             // Completely filled
    PARTIALLY_FILLED,   // Partially filled
    CANCELLED,          // Cancelled by user
    EXPIRED            // Time expired
};

// [SEQUENCE: 1916] Commodity type
enum class CommodityType {
    // Basic materials
    ORE_COPPER,
    ORE_IRON,
    ORE_GOLD,
    ORE_MITHRIL,
    
    // Herbs
    HERB_PEACEBLOOM,
    HERB_SILVERLEAF,
    HERB_SUNGRASS,
    HERB_DREAMFOIL,
    
    // Cloth
    CLOTH_LINEN,
    CLOTH_WOOL,
    CLOTH_SILK,
    CLOTH_MAGEWEAVE,
    
    // Leather
    LEATHER_LIGHT,
    LEATHER_MEDIUM,
    LEATHER_HEAVY,
    LEATHER_THICK,
    
    // Gems
    GEM_RUBY,
    GEM_SAPPHIRE,
    GEM_EMERALD,
    GEM_DIAMOND,
    
    // Consumables
    POTION_HEALTH,
    POTION_MANA,
    FOOD_BREAD,
    FOOD_MEAT,
    
    // Crafting components
    ESSENCE_MAGIC,
    DUST_ARCANE,
    SHARD_SOUL,
    CRYSTAL_POWER
};

// [SEQUENCE: 1917] Market order
struct MarketOrder {
    uint64_t order_id;
    uint64_t player_id;
    std::string player_name;
    
    // Order details
    OrderType type;
    CommodityType commodity;
    uint32_t quantity;
    uint32_t quantity_filled = 0;
    uint64_t price_per_unit;
    
    // Timing
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point expire_time;
    std::chrono::hours duration{24};  // Default 24 hours
    
    // Status
    OrderStatus status = OrderStatus::ACTIVE;
    uint64_t total_cost;              // For buy orders
    uint32_t items_held = 0;          // For sell orders
    
    // [SEQUENCE: 1918] Get remaining quantity
    uint32_t GetRemainingQuantity() const {
        return quantity - quantity_filled;
    }
    
    // [SEQUENCE: 1919] Check if expired
    bool IsExpired() const {
        return std::chrono::system_clock::now() >= expire_time;
    }
    
    // [SEQUENCE: 1920] Can be matched
    bool CanMatch(const MarketOrder& other) const {
        if (status != OrderStatus::ACTIVE || other.status != OrderStatus::ACTIVE) {
            return false;
        }
        
        if (commodity != other.commodity) {
            return false;
        }
        
        if (type == other.type) {
            return false;  // Can't match same type
        }
        
        // Price compatibility
        if (type == OrderType::BUY) {
            return price_per_unit >= other.price_per_unit;
        } else {
            return price_per_unit <= other.price_per_unit;
        }
    }
};

// [SEQUENCE: 1921] Trade execution record
struct TradeExecution {
    uint64_t trade_id;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    uint64_t buyer_id;
    uint64_t seller_id;
    
    CommodityType commodity;
    uint32_t quantity;
    uint64_t price_per_unit;
    uint64_t total_value;
    
    std::chrono::system_clock::time_point execution_time;
};

// [SEQUENCE: 1922] Market statistics
struct MarketStats {
    CommodityType commodity;
    
    // Price data
    uint64_t last_price = 0;
    uint64_t highest_buy = 0;
    uint64_t lowest_sell = 0;
    uint64_t average_price_24h = 0;
    
    // Volume data
    uint32_t volume_24h = 0;
    uint32_t buy_orders_count = 0;
    uint32_t sell_orders_count = 0;
    uint32_t total_supply = 0;
    uint32_t total_demand = 0;
    
    // Price history
    std::deque<std::pair<std::chrono::system_clock::time_point, uint64_t>> price_history;
};

// [SEQUENCE: 1923] Order book for a commodity
class OrderBook {
public:
    OrderBook(CommodityType commodity) : commodity_(commodity) {}
    
    // [SEQUENCE: 1924] Add order to book
    void AddOrder(const MarketOrder& order) {
        if (order.type == OrderType::BUY) {
            buy_orders_.push_back(order);
            // Sort buy orders by price (highest first)
            std::sort(buy_orders_.begin(), buy_orders_.end(),
                [](const MarketOrder& a, const MarketOrder& b) {
                    return a.price_per_unit > b.price_per_unit;
                });
        } else {
            sell_orders_.push_back(order);
            // Sort sell orders by price (lowest first)
            std::sort(sell_orders_.begin(), sell_orders_.end(),
                [](const MarketOrder& a, const MarketOrder& b) {
                    return a.price_per_unit < b.price_per_unit;
                });
        }
    }
    
    // [SEQUENCE: 1925] Match orders
    std::vector<TradeExecution> MatchOrders() {
        std::vector<TradeExecution> executions;
        
        while (!buy_orders_.empty() && !sell_orders_.empty()) {
            auto& buy = buy_orders_.front();
            auto& sell = sell_orders_.front();
            
            // Check if orders can match
            if (!buy.CanMatch(sell)) {
                break;  // No more matches possible
            }
            
            // Execute trade
            uint32_t trade_quantity = std::min(
                buy.GetRemainingQuantity(),
                sell.GetRemainingQuantity()
            );
            
            uint64_t trade_price = sell.price_per_unit;  // Use sell price
            
            TradeExecution execution;
            execution.trade_id = GenerateTradeId();
            execution.buy_order_id = buy.order_id;
            execution.sell_order_id = sell.order_id;
            execution.buyer_id = buy.player_id;
            execution.seller_id = sell.player_id;
            execution.commodity = commodity_;
            execution.quantity = trade_quantity;
            execution.price_per_unit = trade_price;
            execution.total_value = trade_quantity * trade_price;
            execution.execution_time = std::chrono::system_clock::now();
            
            executions.push_back(execution);
            
            // Update orders
            buy.quantity_filled += trade_quantity;
            sell.quantity_filled += trade_quantity;
            
            // Update order status
            if (buy.quantity_filled >= buy.quantity) {
                buy.status = OrderStatus::FILLED;
                buy_orders_.erase(buy_orders_.begin());
            } else {
                buy.status = OrderStatus::PARTIALLY_FILLED;
            }
            
            if (sell.quantity_filled >= sell.quantity) {
                sell.status = OrderStatus::FILLED;
                sell_orders_.erase(sell_orders_.begin());
            } else {
                sell.status = OrderStatus::PARTIALLY_FILLED;
            }
            
            // Update market stats
            UpdateMarketStats(execution);
        }
        
        return executions;
    }
    
    // [SEQUENCE: 1926] Remove expired orders
    void RemoveExpiredOrders() {
        auto now = std::chrono::system_clock::now();
        
        // Remove expired buy orders
        std::erase_if(buy_orders_, [now](const MarketOrder& order) {
            return order.IsExpired();
        });
        
        // Remove expired sell orders
        std::erase_if(sell_orders_, [now](const MarketOrder& order) {
            return order.IsExpired();
        });
    }
    
    // [SEQUENCE: 1927] Get market depth
    struct MarketDepth {
        std::vector<std::pair<uint64_t, uint32_t>> buy_levels;   // price, quantity
        std::vector<std::pair<uint64_t, uint32_t>> sell_levels;  // price, quantity
    };
    
    MarketDepth GetMarketDepth(uint32_t levels = 5) const {
        MarketDepth depth;
        
        // Aggregate buy orders by price
        std::unordered_map<uint64_t, uint32_t> buy_aggregated;
        for (const auto& order : buy_orders_) {
            if (order.status == OrderStatus::ACTIVE) {
                buy_aggregated[order.price_per_unit] += order.GetRemainingQuantity();
            }
        }
        
        // Convert to sorted vector
        for (const auto& [price, quantity] : buy_aggregated) {
            depth.buy_levels.emplace_back(price, quantity);
        }
        std::sort(depth.buy_levels.begin(), depth.buy_levels.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Limit levels
        if (depth.buy_levels.size() > levels) {
            depth.buy_levels.resize(levels);
        }
        
        // Same for sell orders
        std::unordered_map<uint64_t, uint32_t> sell_aggregated;
        for (const auto& order : sell_orders_) {
            if (order.status == OrderStatus::ACTIVE) {
                sell_aggregated[order.price_per_unit] += order.GetRemainingQuantity();
            }
        }
        
        for (const auto& [price, quantity] : sell_aggregated) {
            depth.sell_levels.emplace_back(price, quantity);
        }
        std::sort(depth.sell_levels.begin(), depth.sell_levels.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        
        if (depth.sell_levels.size() > levels) {
            depth.sell_levels.resize(levels);
        }
        
        return depth;
    }
    
    // Query methods
    const std::vector<MarketOrder>& GetBuyOrders() const { return buy_orders_; }
    const std::vector<MarketOrder>& GetSellOrders() const { return sell_orders_; }
    const MarketStats& GetStats() const { return stats_; }
    
private:
    CommodityType commodity_;
    std::vector<MarketOrder> buy_orders_;
    std::vector<MarketOrder> sell_orders_;
    MarketStats stats_;
    
    static std::atomic<uint64_t> next_trade_id_;
    
    uint64_t GenerateTradeId() {
        return next_trade_id_++;
    }
    
    // [SEQUENCE: 1928] Update market statistics
    void UpdateMarketStats(const TradeExecution& execution) {
        stats_.last_price = execution.price_per_unit;
        stats_.volume_24h += execution.quantity;
        
        // Update price history
        stats_.price_history.emplace_back(
            execution.execution_time, 
            execution.price_per_unit
        );
        
        // Keep only 24h of history
        auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24);
        while (!stats_.price_history.empty() && 
               stats_.price_history.front().first < cutoff) {
            stats_.price_history.pop_front();
        }
        
        // Calculate average
        if (!stats_.price_history.empty()) {
            uint64_t sum = 0;
            for (const auto& [time, price] : stats_.price_history) {
                sum += price;
            }
            stats_.average_price_24h = sum / stats_.price_history.size();
        }
        
        // Update bid/ask
        stats_.highest_buy = buy_orders_.empty() ? 0 : buy_orders_.front().price_per_unit;
        stats_.lowest_sell = sell_orders_.empty() ? 0 : sell_orders_.front().price_per_unit;
        
        // Update counts
        stats_.buy_orders_count = buy_orders_.size();
        stats_.sell_orders_count = sell_orders_.size();
    }
};

std::atomic<uint64_t> OrderBook::next_trade_id_{1};

// [SEQUENCE: 1929] Trading post manager
class TradingPost {
public:
    TradingPost(const std::string& name) : post_name_(name) {}
    
    // [SEQUENCE: 1930] Place buy order
    std::optional<uint64_t> PlaceBuyOrder(
        uint64_t player_id,
        const std::string& player_name,
        CommodityType commodity,
        uint32_t quantity,
        uint64_t price_per_unit,
        std::chrono::hours duration = std::chrono::hours(24)
    ) {
        // Validate order
        if (quantity == 0 || price_per_unit == 0) {
            return std::nullopt;
        }
        
        uint64_t total_cost = quantity * price_per_unit;
        
        // Check player has enough money
        if (!HasMoney(player_id, total_cost)) {
            return std::nullopt;
        }
        
        // Deduct money (held in escrow)
        DeductMoney(player_id, total_cost);
        
        // Create order
        MarketOrder order;
        order.order_id = GenerateOrderId();
        order.player_id = player_id;
        order.player_name = player_name;
        order.type = OrderType::BUY;
        order.commodity = commodity;
        order.quantity = quantity;
        order.price_per_unit = price_per_unit;
        order.created_time = std::chrono::system_clock::now();
        order.expire_time = order.created_time + duration;
        order.duration = duration;
        order.total_cost = total_cost;
        
        // Add to order book
        auto& book = GetOrCreateOrderBook(commodity);
        book.AddOrder(order);
        
        // Store order reference
        active_orders_[order.order_id] = order;
        player_orders_[player_id].push_back(order.order_id);
        
        // Try to match immediately
        ProcessMatching(commodity);
        
        spdlog::info("Buy order {} placed: {} x{} @ {} each", 
                    order.order_id, GetCommodityName(commodity), 
                    quantity, price_per_unit);
        
        return order.order_id;
    }
    
    // [SEQUENCE: 1931] Place sell order
    std::optional<uint64_t> PlaceSellOrder(
        uint64_t player_id,
        const std::string& player_name,
        CommodityType commodity,
        uint32_t quantity,
        uint64_t price_per_unit,
        std::chrono::hours duration = std::chrono::hours(24)
    ) {
        // Validate order
        if (quantity == 0 || price_per_unit == 0) {
            return std::nullopt;
        }
        
        // Check player has items
        if (!HasItems(player_id, commodity, quantity)) {
            return std::nullopt;
        }
        
        // Remove items (held in escrow)
        RemoveItems(player_id, commodity, quantity);
        
        // Create order
        MarketOrder order;
        order.order_id = GenerateOrderId();
        order.player_id = player_id;
        order.player_name = player_name;
        order.type = OrderType::SELL;
        order.commodity = commodity;
        order.quantity = quantity;
        order.price_per_unit = price_per_unit;
        order.created_time = std::chrono::system_clock::now();
        order.expire_time = order.created_time + duration;
        order.duration = duration;
        order.items_held = quantity;
        
        // Add to order book
        auto& book = GetOrCreateOrderBook(commodity);
        book.AddOrder(order);
        
        // Store order reference
        active_orders_[order.order_id] = order;
        player_orders_[player_id].push_back(order.order_id);
        
        // Try to match immediately
        ProcessMatching(commodity);
        
        spdlog::info("Sell order {} placed: {} x{} @ {} each", 
                    order.order_id, GetCommodityName(commodity), 
                    quantity, price_per_unit);
        
        return order.order_id;
    }
    
    // [SEQUENCE: 1932] Cancel order
    bool CancelOrder(uint64_t order_id, uint64_t player_id) {
        auto it = active_orders_.find(order_id);
        if (it == active_orders_.end()) {
            return false;
        }
        
        auto& order = it->second;
        
        // Verify ownership
        if (order.player_id != player_id) {
            return false;
        }
        
        // Can't cancel filled orders
        if (order.status == OrderStatus::FILLED) {
            return false;
        }
        
        // Return escrowed funds/items
        if (order.type == OrderType::BUY) {
            uint32_t remaining = order.GetRemainingQuantity();
            uint64_t refund = remaining * order.price_per_unit;
            SendMoney(player_id, refund);
        } else {
            uint32_t remaining = order.GetRemainingQuantity();
            SendItems(player_id, order.commodity, remaining);
        }
        
        // Update status
        order.status = OrderStatus::CANCELLED;
        
        // Remove from active
        completed_orders_[order_id] = order;
        active_orders_.erase(it);
        
        spdlog::info("Order {} cancelled by player {}", order_id, player_id);
        
        return true;
    }
    
    // [SEQUENCE: 1933] Get market data
    MarketStats GetMarketStats(CommodityType commodity) const {
        auto it = order_books_.find(commodity);
        if (it != order_books_.end()) {
            return it->second->GetStats();
        }
        return MarketStats{commodity};
    }
    
    // [SEQUENCE: 1934] Get order book depth
    OrderBook::MarketDepth GetMarketDepth(CommodityType commodity, uint32_t levels = 5) const {
        auto it = order_books_.find(commodity);
        if (it != order_books_.end()) {
            return it->second->GetMarketDepth(levels);
        }
        return OrderBook::MarketDepth{};
    }
    
    // [SEQUENCE: 1935] Get player orders
    std::vector<MarketOrder> GetPlayerOrders(uint64_t player_id) const {
        std::vector<MarketOrder> orders;
        
        auto it = player_orders_.find(player_id);
        if (it != player_orders_.end()) {
            for (uint64_t order_id : it->second) {
                auto order_it = active_orders_.find(order_id);
                if (order_it != active_orders_.end()) {
                    orders.push_back(order_it->second);
                }
            }
        }
        
        return orders;
    }
    
    // [SEQUENCE: 1936] Process all commodities
    void ProcessAllMatching() {
        for (auto& [commodity, book] : order_books_) {
            ProcessMatching(commodity);
        }
    }
    
    // [SEQUENCE: 1937] Clean expired orders
    void CleanExpiredOrders() {
        for (auto& [commodity, book] : order_books_) {
            book->RemoveExpiredOrders();
        }
        
        // Return funds/items for expired orders
        std::vector<uint64_t> to_remove;
        for (auto& [order_id, order] : active_orders_) {
            if (order.IsExpired() && order.status != OrderStatus::FILLED) {
                // Return escrow
                if (order.type == OrderType::BUY) {
                    uint32_t remaining = order.GetRemainingQuantity();
                    uint64_t refund = remaining * order.price_per_unit;
                    SendMoney(order.player_id, refund);
                } else {
                    uint32_t remaining = order.GetRemainingQuantity();
                    SendItems(order.player_id, order.commodity, remaining);
                }
                
                order.status = OrderStatus::EXPIRED;
                to_remove.push_back(order_id);
            }
        }
        
        // Move to completed
        for (uint64_t order_id : to_remove) {
            completed_orders_[order_id] = active_orders_[order_id];
            active_orders_.erase(order_id);
        }
    }
    
private:
    std::string post_name_;
    std::unordered_map<CommodityType, std::unique_ptr<OrderBook>> order_books_;
    std::unordered_map<uint64_t, MarketOrder> active_orders_;
    std::unordered_map<uint64_t, MarketOrder> completed_orders_;
    std::unordered_map<uint64_t, std::vector<uint64_t>> player_orders_;
    
    static std::atomic<uint64_t> next_order_id_;
    
    uint64_t GenerateOrderId() {
        return next_order_id_++;
    }
    
    // [SEQUENCE: 1938] Get or create order book
    OrderBook& GetOrCreateOrderBook(CommodityType commodity) {
        auto it = order_books_.find(commodity);
        if (it == order_books_.end()) {
            order_books_[commodity] = std::make_unique<OrderBook>(commodity);
            it = order_books_.find(commodity);
        }
        return *it->second;
    }
    
    // [SEQUENCE: 1939] Process matching for commodity
    void ProcessMatching(CommodityType commodity) {
        auto& book = GetOrCreateOrderBook(commodity);
        auto executions = book.MatchOrders();
        
        for (const auto& execution : executions) {
            // Transfer money from buyer to seller
            uint64_t seller_payment = execution.total_value;
            SendMoney(execution.seller_id, seller_payment);
            
            // Transfer items from escrow to buyer
            SendItems(execution.buyer_id, commodity, execution.quantity);
            
            // Update order status in our records
            UpdateOrderStatus(execution.buy_order_id, execution.quantity);
            UpdateOrderStatus(execution.sell_order_id, execution.quantity);
            
            spdlog::info("Trade executed: {} x{} @ {} between {} and {}", 
                        GetCommodityName(commodity), execution.quantity,
                        execution.price_per_unit, execution.buyer_id, 
                        execution.seller_id);
        }
    }
    
    // [SEQUENCE: 1940] Update order status after execution
    void UpdateOrderStatus(uint64_t order_id, uint32_t filled_quantity) {
        auto it = active_orders_.find(order_id);
        if (it != active_orders_.end()) {
            auto& order = it->second;
            order.quantity_filled += filled_quantity;
            
            if (order.quantity_filled >= order.quantity) {
                order.status = OrderStatus::FILLED;
                completed_orders_[order_id] = order;
                active_orders_.erase(it);
            } else {
                order.status = OrderStatus::PARTIALLY_FILLED;
            }
        }
    }
    
    // [SEQUENCE: 1941] Get commodity name
    std::string GetCommodityName(CommodityType commodity) const {
        // TODO: Return proper names
        return "Commodity";
    }
    
    // Helper methods (to be implemented by economy system)
    bool HasMoney(uint64_t player_id, uint64_t amount);
    void DeductMoney(uint64_t player_id, uint64_t amount);
    void SendMoney(uint64_t player_id, uint64_t amount);
    bool HasItems(uint64_t player_id, CommodityType commodity, uint32_t quantity);
    void RemoveItems(uint64_t player_id, CommodityType commodity, uint32_t quantity);
    void SendItems(uint64_t player_id, CommodityType commodity, uint32_t quantity);
};

std::atomic<uint64_t> TradingPost::next_order_id_{1};

// [SEQUENCE: 1942] Global trading post manager
class TradingPostManager {
public:
    static TradingPostManager& Instance() {
        static TradingPostManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1943] Initialize trading posts
    void Initialize() {
        // Create regional trading posts
        CreateTradingPost("Stormwind Trading Post");
        CreateTradingPost("Orgrimmar Trading Post");
        CreateTradingPost("Neutral Trading Post");
        
        spdlog::info("Trading post system initialized with {} posts", 
                    trading_posts_.size());
    }
    
    // [SEQUENCE: 1944] Get trading post
    std::shared_ptr<TradingPost> GetTradingPost(const std::string& name) {
        auto it = trading_posts_.find(name);
        return (it != trading_posts_.end()) ? it->second : nullptr;
    }
    
    // [SEQUENCE: 1945] Update all trading posts
    void UpdateAll() {
        for (auto& [name, post] : trading_posts_) {
            post->ProcessAllMatching();
            post->CleanExpiredOrders();
        }
    }
    
private:
    TradingPostManager() = default;
    
    std::unordered_map<std::string, std::shared_ptr<TradingPost>> trading_posts_;
    
    void CreateTradingPost(const std::string& name) {
        trading_posts_[name] = std::make_shared<TradingPost>(name);
    }
};

// [SEQUENCE: 1946] Stub implementations
bool TradingPost::HasMoney(uint64_t player_id, uint64_t amount) {
    // TODO: Check player's money
    return true;
}

void TradingPost::DeductMoney(uint64_t player_id, uint64_t amount) {
    // TODO: Deduct from player's money
}

void TradingPost::SendMoney(uint64_t player_id, uint64_t amount) {
    // TODO: Add to player's money
}

bool TradingPost::HasItems(uint64_t player_id, CommodityType commodity, uint32_t quantity) {
    // TODO: Check player's inventory
    return true;
}

void TradingPost::RemoveItems(uint64_t player_id, CommodityType commodity, uint32_t quantity) {
    // TODO: Remove from player's inventory
}

void TradingPost::SendItems(uint64_t player_id, CommodityType commodity, uint32_t quantity) {
    // TODO: Add to player's inventory
}

} // namespace mmorpg::game::economy