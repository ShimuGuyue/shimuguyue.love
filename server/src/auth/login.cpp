/**
 * @file auth/login.cpp
 * @brief 用户登录认证实现
 */

#include "auth/login.h"

#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

#include "crypto/argon2id.h"

namespace
{
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





namespace auth
{
    auto login_by_key(
        pqxx::connection& conn, std::string_view key)
    -> std::expected<LoginResult, std::string>
    {
        spdlog::debug("正在进行密钥登录...");
        // Step 1: 验证密钥非空
        if (key.empty())
        {
            spdlog::info("登录失败：密钥为空。");
            return std::unexpected(std::string{ "密钥不能为空" });
        }

        // Step 2: 使用固定盐值对 key 做一次哈希
        const auto hash = crypto::Argon2id::hash_with_fixed_salt(key);
        if (!hash)
        {
            spdlog::error("系统错误：固定盐值 Argon2id 哈希失败！");
            return std::unexpected(std::string{ "系统出了点问题，请稍后再试" });
        }

        pqxx::work txn{ conn };

        // Step 3: 在数据库中精确查找该哈希值（仅生效且可用的密钥）
        const auto row = txn.exec(
            "SELECT id, username "
            "FROM users "
            "WHERE key_hash = $1 AND key_enabled = true AND enabled = true",
            pqxx::params{ *hash }
        );

        if (row.empty())
        {
            spdlog::info("登录失败：无效的密钥。");
            return std::unexpected(std::string{ "无效的密钥" });
        }

        // Step 4: 构建登录结果并查询用户权限
        LoginResult result;
        result.id = row[0]["id"].as<int>();
        if (!row[0]["username"].is_null())
            result.username = row[0]["username"].as<std::string>();
        result.permissions = fetch_permissions(txn, result.id);
        txn.commit();
        spdlog::debug("密钥登录完成：{}。", *hash);
        return result;
    }

    auto login_by_password(
        pqxx::connection& conn,
        std::string_view  username,
        std::string_view  password)
    -> std::expected<LoginResult, std::string>
    {
        spdlog::debug("正在进行用户名密码登录...");
        // 验证用户名和密码非空
        if (username.empty() || password.empty())
        {
            spdlog::info("登录失败：用户名或密码为空。");
            return std::unexpected(std::string{ "用户名或密码不能为空" });
        }

        pqxx::work txn{ conn };

        // 在数据库中查找用户记录（仅可用用户）
        const auto row = txn.exec( 
            "SELECT id, username, password_hash "
            "FROM users "
            "WHERE username = $1 AND enabled = true",
            pqxx::params{ std::string{ username } }
        );

        if (row.empty())
        {
            spdlog::info("登录失败：用户 {} 不存在。", username);
            return std::unexpected(std::string{ "用户名或密码错误" });
        }

        // 验证密码哈希
        const auto hash = row[0]["password_hash"];

        if (hash.is_null())
        {
            spdlog::info("登录失败：用户 {} 未设置密码。", username);
            return std::unexpected(std::string{ "用户名或密码错误" });
        }

        // 使用 Argon2id 验证密码
        const auto hash_str = hash.as<std::string>();
        const auto verify_result = crypto::Argon2id::verify_with_random_salt(password, hash_str);
        if (verify_result == crypto::VerifyResult::Mismatch)
        {
            spdlog::info("登录失败：用户 {} 密码错误。", username);
            return std::unexpected(std::string{ "用户名或密码错误" });
        }
        if (verify_result == crypto::VerifyResult::SystemError)
        {
            spdlog::error("系统错误：用户 {} 密码验证失败。", username);
            return std::unexpected(std::string{ "系统出了点问题，请稍后再试" });
        }

        // 构建登录结果并查询用户权限
        LoginResult result;
        result.id = row[0]["id"].as<int>();
        if (!row[0]["username"].is_null())
        {
            result.username = row[0]["username"].as<std::string>();
        }
        result.permissions = fetch_permissions(txn, result.id);
        txn.commit();
        spdlog::debug("密码登录完成。");
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
