/**
 * @file cache/cache.cpp
 * @brief Redis 公开接口缓存模块实现
 */

#include "cache/cache.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>
#include <sw/redis++/redis++.h>

#include "config/env.h"

namespace
{
    /// Redis 客户端（内部连接池，连接数由 REDIS_POOL_SIZE 决定）。
    std::unique_ptr<sw::redis::Redis> g_redis;

    /// 连接与读写超时时长。
    constexpr std::chrono::milliseconds IO_TIMEOUT{ 5000 };

    /**
     * @brief 记录 Redis 运行期故障日志（调用方降级直查数据库）。
     * @param op  失败的操作名。
     * @param why 失败原因。
     */
    void log_redis_error(const char* op, std::string_view why)
    {
        spdlog::warn("Redis 操作 {} 失败，降级直查数据库：{}", op, why);
    }

    /**
     * @brief 对缓存键片段做最小百分号转义。
     *
     * 转义 %、&、=、? 四个字符，避免不同参数结构拼出相同的缓存键；
     * 键仅作为不透明 Redis 字符串使用，无需解码。
     * @param text 原始片段。
     * @return 转义后的片段。
     */
    [[nodiscard]] auto escape_key_component(std::string_view text) -> std::string
    {
        std::string out;
        out.reserve(text.size());
        for (const char c : text)
        {
            switch (c)
            {
            case '%':
                out.append("%25");
                break;
            case '&':
                out.append("%26");
                break;
            case '=':
                out.append("%3D");
                break;
            case '?':
                out.append("%3F");
                break;
            default:
                out.push_back(c);
                break;
            }
        }
        return out;
    }
} // namespace





namespace cache
{
    void init()
    {
        // 为了安全使用 stoi / stoull。必须保证调用 cache::init() 前首先调用
        // config::init_env() 校验 REDIS_PORT 为 1~65535、REDIS_POOL_SIZE 为正整数，
        const auto host      =             config::env()["REDIS_HOST"];
        const auto password  =             config::env()["REDIS_PASSWORD"];
        const auto port      = std::stoi  (config::env()["REDIS_PORT"]);
        const auto pool_size = std::stoull(config::env()["REDIS_POOL_SIZE"]);

        sw::redis::ConnectionOptions conn_opts;
        conn_opts.host            = host;
        conn_opts.port            = port;
        conn_opts.password        = password;
        conn_opts.connect_timeout = IO_TIMEOUT;
        conn_opts.socket_timeout  = IO_TIMEOUT;

        sw::redis::ConnectionPoolOptions pool_opts;
        pool_opts.size         = pool_size;
        pool_opts.wait_timeout = IO_TIMEOUT;

        g_redis = std::make_unique<sw::redis::Redis>(conn_opts, pool_opts);

        try
        {
            const std::string pong{ g_redis->ping() };
            spdlog::info("Redis 连接成功（{}:{}）{}。", host, port, pong);
        }
        catch (const sw::redis::Error& e)
        {
            spdlog::error("Redis 连接失败（{}:{}）：{}", host, port, e.what());
            std::exit(1);
        }
    }

    auto get(const std::string& key) -> std::optional<std::string>
    {
        try
        {
            auto value = g_redis->get(key);
            if (value)
            {
                spdlog::info("缓存命中，直接返回：{}", key);
                return value.value();
            }
            spdlog::info("缓存未命中，转直查数据库：{}", key);
            return std::nullopt;
        }
        catch (const sw::redis::Error& e)
        {
            log_redis_error("GET", e.what());
            return std::nullopt;
        }
    }

    auto set(
        const std::string& key,
        const std::string& value,
        long long          ttl_seconds) -> bool
    {
        try
        {
            g_redis->set(key, value, std::chrono::seconds(ttl_seconds));
            spdlog::debug("已从数据库取数并写入缓存：{}（TTL {} 秒）", key, ttl_seconds);
            return true;
        }
        catch (const sw::redis::Error& e)
        {
            log_redis_error("SET", e.what());
            return false;
        }
    }

    auto del(const std::string& key) -> bool
    {
        try
        {
            return g_redis->del(key) > 0;
        }
        catch (const sw::redis::Error& e)
        {
            log_redis_error("DEL", e.what());
            return false;
        }
    }

    void invalidate_prefix(std::string_view prefix)
    {
        std::string pattern{ prefix };
        pattern.push_back('*');

        try
        {
            std::unordered_set<std::string> keys;
            sw::redis::Cursor cursor{ 0 };
            do
            {
                cursor = g_redis->scan(
                    cursor,
                    pattern,
                    100,
                    std::inserter(keys, keys.begin())
                );
            } while (cursor != 0);

            if (keys.empty())
                return;

            const int64_t removed{ g_redis->del(keys.begin(), keys.end()) };
            spdlog::debug("缓存失效：前缀 {} 删除 {} 个键。", prefix, removed);
        }
        catch (const sw::redis::Error& e)
        {
            log_redis_error("SCAN/DEL", e.what());
        }
    }

    auto cache_key(
        std::string_view                                    path,
        const std::unordered_map<std::string, std::string>& params) -> std::string
    {
        std::string key{ "api-cache:" };
        key.append(path);

        if (params.empty())
        {
            return key;
        }

        std::vector<std::pair<std::string, std::string>> items{ params.begin(), params.end() };
        std::sort(
            items.begin(),
            items.end(),
            [](const auto& a, const auto& b)
            {
                return a.first != b.first
                    ?  a.first < b.first
                    :  a.second < b.second;
            }
        );

        key.push_back('?');
        bool first = true;
        for (const auto& item : items)
        {
            if (!first)
            {
                key.push_back('&');
            }
            first = false;
            key.append(escape_key_component(item.first));
            key.push_back('=');
            key.append(escape_key_component(item.second));
        }
        return key;
    }

} // namespace cache
