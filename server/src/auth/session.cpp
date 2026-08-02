/**
 * @file auth/session.cpp
 * @brief 基于数据库的 token 会话管理实现
 */

#include "auth/session.h"

#include <mutex>
#include <random>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace
{

std::mutex g_rng_mutex;

/// 生成 64 字符随机十六进制 token
auto generate_token() -> std::string
{
    static std::mt19937_64 rng{ std::random_device{}() };
    static std::uniform_int_distribution<unsigned long long> dist;

    std::lock_guard lock{ g_rng_mutex };

    std::string token;
    token.reserve(64);
    constexpr char hex[] = "0123456789abcdef";

    for (int i = 0; i < 16; ++i)
    {
        const auto v = dist(rng);
        token.push_back(hex[(v >>  4) & 0xF]);
        token.push_back(hex[(v >> 12) & 0xF]);
        token.push_back(hex[(v >> 20) & 0xF]);
        token.push_back(hex[(v >> 28) & 0xF]);
    }
    return token;
}

} // namespace



namespace auth
{

auto create_session(
    pqxx::connection&               conn,
    int                             user_id,
    const std::vector<std::string>& permissions)
-> std::string
{
    spdlog::info("正在为用户 {} 创建 session...", user_id);
    const auto token = generate_token();

    pqxx::work txn{ conn };

    // 清理该用户的旧 session
    txn.exec("DELETE FROM sessions WHERE user_id = $1",
             pqxx::params{ user_id });

    // 插入新 session
    txn.exec(
        "INSERT INTO sessions (token, user_id, permissions, expires_at) "
        "VALUES ($1, $2, $3, NOW() + INTERVAL '24 hours')",
        pqxx::params{
            token,
            user_id,
            nlohmann::json(permissions).dump()
        }
    );

    txn.commit();
    spdlog::info("用户 {} 的 session 创建成功。", user_id);
    spdlog::debug("session token：{}", token);
    return token;
}

auto validate_session(
    pqxx::connection& conn,
    std::string_view  token)
-> std::optional<SessionInfo>
{
    spdlog::debug("正在验证 session...");
    pqxx::work txn{ conn };

    txn.exec(
        "DELETE FROM sessions "
        "WHERE expires_at <= NOW()"
    );

    const auto result = txn.exec(
        "SELECT user_id, permissions FROM sessions "
        "WHERE token = $1 AND expires_at > NOW()",
        pqxx::params{ std::string{token} }
    );

    if (result.empty())
    {
        txn.commit();
        spdlog::info("session 验证失败：token 无效或已过期。");
        return std::nullopt;
    }

    SessionInfo info;
    info.user_id = result[0]["user_id"].as<int>();

    const auto perms_json = nlohmann::json::parse(
        result[0]["permissions"].as<std::string>(), nullptr, false
    );
    if (perms_json.is_array())
    {
        for (const auto& p : perms_json)
            if (p.is_string())
                info.permissions.push_back(p.get<std::string>());
    }

    txn.commit();
    spdlog::debug("session 验证通过（user_id={}）。", info.user_id);
    return info;
}

void cleanup_expired_sessions(pqxx::connection& conn)
{
    spdlog::info("正在清理过期 session...");
    pqxx::work txn{ conn };
    const auto r = txn.exec("DELETE FROM sessions WHERE expires_at <= NOW()");
    txn.commit();
    spdlog::info("已清理 {} 条过期 session。", r.affected_rows());
}

} // namespace auth
