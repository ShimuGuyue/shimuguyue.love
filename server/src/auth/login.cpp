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
    auto get_permissions(pqxx::work& txn, int user_id) -> std::vector<std::string>
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
        // 验证密钥非空
        if (key.empty())
            return std::unexpected("密钥不能为空");

        // 对 key 进行固定盐哈希
        const auto hash = crypto::Argon2id::hash_with_fixed_salt(key);
        if (!hash)
            return std::unexpected("系统故障，请稍后再试");

        pqxx::work txn{ conn };

        //  在数据库中精确查找启用的密钥哈希
        const auto row = txn.exec(
            "SELECT id, username "
            "FROM users "
            "WHERE key_hash = $1 AND key_enabled = true AND enabled = true",
            pqxx::params{ *hash }
        );

        if (row.empty())
            return std::unexpected("无效的密钥");

        // Step 4: 构建登录结果并查询用户权限
        LoginResult result;
        result.id = row[0]["id"].as<int>();
        if (!row[0]["username"].is_null())
            result.username = row[0]["username"].as<std::string>();
        result.permissions = get_permissions(txn, result.id);
        txn.commit();

        return result;
    }

    auto login_by_password(
        pqxx::connection& conn,
        std::string_view  username,
        std::string_view  password)
    -> std::expected<LoginResult, std::string>
    {
        // 验证用户名和密码非空
        if (username.empty() || password.empty())
            return std::unexpected("用户名或密码不能为空");

        pqxx::work txn{ conn };

        // 在数据库中查找用户
        const auto rows = txn.exec(
            "SELECT id, username, password_hash "
            "FROM users "
            "WHERE username = $1 AND enabled = true",
            pqxx::params{ std::string{ username } }
        );
        if (rows.empty())
            return std::unexpected("用户名或密码错误");

        // 验证密码哈希
        auto row = rows[0];
        const auto hash = row["password_hash"];
        if (hash.is_null())
            return std::unexpected("用户名或密码错误");

        // 使用 Argon2id 验证密码
        const auto hash_str = hash.as<std::string>();
        const auto verify_result = crypto::Argon2id::verify_with_random_salt(password, hash_str);
        if (verify_result == crypto::VerifyResult::Mismatch)
            return std::unexpected("用户名或密码错误");
        if (verify_result == crypto::VerifyResult::SystemError)
            return std::unexpected(std::string{ "系统故障，请稍后再试" });

        // 构建登录结果并查询用户权限
        LoginResult result;
        result.id = row["id"].as<int>();
        if (!row["username"].is_null())
            result.username = row["username"].as<std::string>();
        result.permissions = get_permissions(txn, result.id);
        txn.commit();

        return result;
    }

} // namespace auth
