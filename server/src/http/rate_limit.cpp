/**
 * @file http/rate_limit.cpp
 * @brief 登录频率限制实现
 */

#include "http/rate_limit.h"

#include <chrono>
#include <unordered_map>
#include <vector>

namespace rate_limit {

/// 冷却时间内允许的最大失败尝试次数
constexpr int MAX_ATTEMPTS = 5;

/// 登录失败后的冷却时间（秒）
constexpr int COOLDOWN_SECONDS = 60;

/// 每个 IP 的失败尝试时间戳记录
static std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> g_attempts;

auto is_rate_limited(const std::string& ip) -> bool
{
    auto&       timestamps = g_attempts[ip];
    const auto  now        = std::chrono::steady_clock::now();
    const auto  cutoff     = now - std::chrono::seconds(COOLDOWN_SECONDS);

    // 清理过期记录
    std::erase_if(timestamps, [cutoff](const auto& t) { return t < cutoff; });

    return timestamps.size() >= static_cast<std::size_t>(MAX_ATTEMPTS);
}

void record_failure(const std::string& ip)
{
    g_attempts[ip].push_back(std::chrono::steady_clock::now());
}

void clear(const std::string& ip)
{
    g_attempts.erase(ip);
}

} // namespace rate_limit
