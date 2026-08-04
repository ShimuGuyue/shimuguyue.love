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
constexpr int MAX_ATTEMPTS = 5;                                                                        ///< 冷却窗口内允许的最大失败尝试次数。
constexpr int COOLDOWN_SECONDS = 60;                                                                   ///< 登录失败后的冷却时间（秒）。
static std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> g_attempts; ///< 每个 IP 的失败尝试时间戳记录

    /**
     * @brief 基于滑动窗口的登录限流检查。
     *        维护每个 IP 在冷却窗口内的失败尝试时间戳列表，
     *        当窗口内失败次数 >= MAX_ATTEMPTS 时触发限流。
     */
    auto is_rate_limited(const std::string& ip) -> bool
    {
        auto&       timestamps = g_attempts[ip];
        const auto  now        = std::chrono::steady_clock::now();
        const auto  cutoff     = now - std::chrono::seconds(COOLDOWN_SECONDS);

        // 清除冷却窗口之外的旧时间戳
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

    /**
     * @brief 记录一次登录失败，将当前时间戳加入对应 IP 的记录列表。
     */
    void record_failure(const std::string& ip)
    {
        g_attempts[ip].push_back(std::chrono::steady_clock::now());
        spdlog::debug("IP {} 记录一次登录失败（当前失败次数：{}）", ip, g_attempts[ip].size());
    }

    /**
     * @brief 清除指定 IP 的所有限流记录。
     */
    void clear(const std::string& ip)
    {
        g_attempts.erase(ip);
        spdlog::debug("IP {} 的限流记录已清除。", ip);
    }

} // namespace auth
