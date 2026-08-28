/**
 * @file cache/cache.h
 * @brief Redis 公开接口缓存模块
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cache
{
    /**
     * @brief 初始化 Redis 客户端并校验连通性。
     *
     * 读取 REDIS_HOST / REDIS_PORT / REDIS_PASSWORD / REDIS_POOL_SIZE
     * 环境变量并构造带连接池的客户端；执行 PING 失败则打印错误并 exit(1)。
     */
    void init();

    /**
     * @brief 读取缓存值。
     * @param key 缓存键。
     * @return 命中时返回缓存字符串；未命中或 Redis 故障返回 std::nullopt。
     */
    [[nodiscard]] auto get(const std::string& key) -> std::optional<std::string>;

    /**
     * @brief 写入缓存并设置过期时间。
     * @param key         缓存键。
     * @param value       缓存值。
     * @param ttl_seconds 过期秒数。
     * @return 写入成功返回 true；Redis 故障返回 false。
     */
    auto set(
        const std::string& key,
        const std::string& value,
        long long          ttl_seconds) -> bool;

    /**
     * @brief 精确删除一个缓存键。
     * @param key 缓存键。
     * @return 删除成功返回 true；Redis 故障返回 false。
     */
    auto del(const std::string& key) -> bool;

    /**
     * @brief 删除所有以指定前缀开头的缓存键（SCAN + DEL）。
     * @param prefix 键前缀，例如 "api-cache:/api/blogs"。
     */
    void invalidate_prefix(std::string_view prefix);

    /**
     * @brief 构造统一前缀的缓存键。
     *
     * 查询参数按键名、键值排序后拼接为 "?k1=v1&k2=v2"，保证参数顺序不影响命中；
     * 参数名与参数值中的 %、&、=、? 会被百分号转义，防止不同参数结构碰撞出相同键。
     * @param path   API 路径，例如 "/api/blogs"。
     * @param params 查询参数键值对。
     * @return 形如 "api-cache:/api/blogs?q=xx&tag_ids=1,2" 的缓存键。
     */
    [[nodiscard]] auto cache_key(
        std::string_view                                    path,
        const std::unordered_map<std::string, std::string>& params) -> std::string;

} // namespace cache
