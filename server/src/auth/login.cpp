/**
 * @file auth/login.cpp
 * @brief 用户登录认证实现
 */

#include "auth/login.h"
#include "crypto/argon2id.h"

#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

namespace auth {

namespace {

/**
 * @brief 查询用户的权限列表。
 * @param txn     当前事务。
 * @param user_id 用户 ID。
 * @return 权限列表。
 */
auto fetch_permissions(pqxx::work& txn, int user_id) -> std::vector<std::string>
{
    const auto rows = txn.exec(
        "SELECT p.name "
        "FROM user_permissions up "
        "JOIN permissions p ON p.id = up.permission_id "
        "WHERE up.user_id = $1 "
        "ORDER BY up.user_id, up.permission_id",
        pqxx::params{ user_id }
    );

    std::vector<std::string> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
    {
        result.emplace_back(row["name"].as<std::string>());
    }
    return result;
}

} // namespace

auto login_by_key(
    pqxx::connection& conn, std::string_view key)
-> std::expected<LoginResult, std::string>
{
    spdlog::info("正在进行密钥登录...");
    if (key.empty())
    {
        spdlog::info("登录失败：密钥为空。");
        return std::unexpected(std::string{ "密钥不能为空" });
    }

    // 使用固定盐值对 key 做一次哈希，然后数据库精确查找
    const auto hash = crypto::Argon2id::hash_with_fixed_salt(key);
    if (!hash)
    {
        spdlog::error("系统错误：密钥哈希失败！");
        return std::unexpected(std::string{ "系统出了点问题，请稍后再试" });
    }

    pqxx::work txn{ conn };

    // 先查该哈希是否存在（不关注是否启用）
    const auto row = txn.exec(
        "SELECT id, username, key_enabled "
        "FROM users "
        "WHERE key_hash = $1 AND key_enabled = true",
        pqxx::params{ *hash }
    );

    if (row.empty())
    {
        spdlog::info("登录失败：无效的密钥。");
        return std::unexpected(std::string{ "无效的密钥" });
    }

    LoginResult result;
    result.id = row[0]["id"].as<int>();
    if (!row[0]["username"].is_null())
        result.username = row[0]["username"].as<std::string>();
    result.permissions = fetch_permissions(txn, result.id);
    txn.commit();
    spdlog::info("密钥登录完成。");
    return result;
}

auto login_by_password(
    pqxx::connection& conn,
    std::string_view  username,
    std::string_view  password)
-> std::expected<LoginResult, std::string>
{
    spdlog::info("正在进行密码登录...");
    if (username.empty() || password.empty())
    {
        spdlog::info("登录失败：用户名或密码为空。");
        return std::unexpected(std::string{ "用户名或密码不能为空" });
    }

    pqxx::work txn{ conn };

    const auto row = txn.exec( 
        "SELECT id, username, password_hash "
        "FROM users "
        "WHERE username = $1",
        pqxx::params{ std::string{ username } }
    );

    if (row.empty())
    {
        spdlog::info("登录失败：用户 {} 不存在。", username);
        return std::unexpected(std::string{ "用户名或密码错误" });
    }

    const auto hash = row[0]["password_hash"];

    if (hash.is_null())
    {
        spdlog::info("登录失败：用户 {} 未设置密码。", username);
        return std::unexpected(std::string{ "用户名或密码错误" });
    }

    const auto hash_str = hash.as<std::string>();
    if (!crypto::Argon2id::verify(password, hash_str))
    {
        spdlog::info("登录失败：用户 {} 密码错误。", username);
        return std::unexpected(std::string{ "用户名或密码错误" });
    }

    LoginResult result;
    result.id = row[0]["id"].as<int>();
    if (!row[0]["username"].is_null())
    {
        result.username = row[0]["username"].as<std::string>();
    }
    result.permissions = fetch_permissions(txn, result.id);
    txn.commit();
    spdlog::info("密码登录完成。");
    return result;
}

auto get_permissions(pqxx::connection& conn, int user_id) -> std::vector<std::string>
{
    pqxx::work txn{ conn };
    auto perms = fetch_permissions(txn, user_id);
    txn.commit();
    return perms;
}

} // namespace auth
