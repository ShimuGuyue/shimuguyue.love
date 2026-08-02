/**
 * @file auth/rate_limit.cpp
 * @brief 登录频率限制实现
 */

#include "auth/rate_limit.h"

#include <chrono>
#include <unordered_map>
#include <vector>
#include <spdlog/spdlog.h>

namespace auth
{

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

    std::erase_if(timestamps,
        [cutoff](const auto& t)
        {
            return t < cutoff;
        }
    );

    const bool limited{ timestamps.size() >= static_cast<std::size_t>(MAX_ATTEMPTS) };
    if (limited)
        spdlog::info("IP {} 登录已被限流。", ip, timestamps.size());
    return limited;
}

void record_failure(const std::string& ip)
{
    g_attempts[ip].push_back(std::chrono::steady_clock::now());
    spdlog::debug("IP {} 记录一次登录失败（当前失败次数：{}）", ip, g_attempts[ip].size());
}

void clear(const std::string& ip)
{
    g_attempts.erase(ip);
    spdlog::debug("IP {} 的限流记录已清除。", ip);
}

} // namespace auth
