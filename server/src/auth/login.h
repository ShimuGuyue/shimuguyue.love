/**
 * @file auth/login.h
 * @brief 用户登录认证函数定义
 */
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <pqxx/pqxx>

namespace auth
{
    /**
     * @brief 登录成功后的返回结果。
     */
    struct LoginResult
    {
        int                        id;
        std::optional<std::string> username;
        std::vector<std::string>   permissions;
    };

    /**
     * @brief 通过密钥登录。
     * @details 使用固定盐值 Argon2id 对密钥做一次哈希，然后数据库精确匹配。
     *          仅匹配存在且启用的密钥。
     * @param conn 数据库连接。
     * @param key  明文密钥。
     * @return 成功返回 LoginResult；失败返回错误消息字符串。
     */
    [[nodiscard]] auto login_by_key(
        pqxx::connection& conn, std::string_view key)
    -> std::expected<LoginResult, std::string>;

    /**
     * @brief 通过用户名密码登录。
     * @details 通过 username 精确定位用户行，用 Argon2id 验证密码。
     * @param conn     数据库连接。
     * @param username 用户名。
     * @param password 明文密码。
     * @return 成功返回 LoginResult；失败返回错误消息字符串。
     */
    [[nodiscard]] auto login_by_password(
        pqxx::connection& conn,
        std::string_view  username,
        std::string_view  password)
    -> std::expected<LoginResult, std::string>;

} // namespace auth
