#include "database/cache_manager.h"
#include "database/query_optimizer.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// [SEQUENCE: MVP7-38] Unit tests for database systems.
TEST(CacheManagerTest, GetOrCreateCache) {
    auto& manager = mmorpg::database::CacheManager::Instance();
    auto cache1 = manager.GetOrCreateCache("test_cache");
    ASSERT_NE(cache1, nullptr);

    auto cache2 = manager.GetOrCreateCache("test_cache");
    EXPECT_EQ(cache1, cache2);

    auto cache3 = manager.GetOrCreateCache("another_cache");
    EXPECT_NE(cache1, cache3);
}

TEST(CacheTest, PutAndGet) {
    mmorpg::database::Cache cache;
    cache.Put("key1", "value1", std::chrono::seconds(5));
    
    auto value = cache.Get("key1");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "value1");

    auto missing_value = cache.Get("non_existent_key");
    EXPECT_FALSE(missing_value.has_value());
}

TEST(CacheTest, Remove) {
    mmorpg::database::Cache cache;
    cache.Put("key1", "value1", std::chrono::seconds(5));
    
    cache.Remove("key1");
    auto value = cache.Get("key1");
    EXPECT_FALSE(value.has_value());
}

TEST(CacheTest, EvictExpired) {
    mmorpg::database::Cache cache;
    cache.Put("key1", "value1", std::chrono::seconds(1));
    cache.Put("key2", "value2", std::chrono::seconds(10));

    std::this_thread::sleep_for(std::chrono::seconds(2));

    cache.EvictExpired();

    auto value1 = cache.Get("key1");
    EXPECT_FALSE(value1.has_value());

    auto value2 = cache.Get("key2");
    ASSERT_TRUE(value2.has_value());
    EXPECT_EQ(*value2, "value2");
}

TEST(QueryOptimizerTest, RegisterAndGetQuery) {
    auto& optimizer = mmorpg::database::QueryOptimizer::Instance();
    std::string query_sql = "SELECT * FROM users WHERE id = ?;";
    optimizer.RegisterQuery("get_user_by_id", query_sql);

    EXPECT_EQ(optimizer.GetQuery("get_user_by_id"), query_sql);
    EXPECT_EQ(optimizer.GetQuery("non_existent_query"), "");
}
