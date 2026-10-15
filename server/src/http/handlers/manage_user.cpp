/**
 * @file http/handlers/manage_user.cpp
 * @brief 后台用户管理 HTTP 路由处理函数实现
 */

#include "http/handlers/manage_user.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "auth/session.h"
#include "crypto/argon2id.h"
#include "db/connection_pool.h"
#include "http/handlers/common.h"

namespace http
{
    void handle_manage_users(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "用户列表获取";
        spdlog::debug("收到 {} 请求。", ACTION);

        nlohmann::json json;

        with_db(res, allowed, ACTION,
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                // Session 验证
                std::string token;  // 提取 Bearer token
                if (req.has_header("Authorization"))
                {
                    const auto& auth_hdr = req.get_header_value("Authorization");
                    constexpr std::string_view PREFIX = "Bearer ";
                    if (auth_hdr.size() > PREFIX.size()
                    &&  auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                        token = auth_hdr.substr(PREFIX.size());
                }

                const auto session = auth::validate_session(conn, token);
                if (!session)
                {
                    spdlog::debug("{}失败：未登录或会话已过期。", ACTION);

                    res.status = 401;

                    json["error"] = "未登录或会话已过期";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 权限检查：manage:view
                const auto& permissions = session->permissions;
                if (std::find(permissions.begin(), permissions.end(), "manage:view") == permissions.end())
                {
                    spdlog::debug("{}失败：用户 {} 无 manage:view 权限。", ACTION, session->user_id);

                    res.status = 403;

                    json["error"] = "当前用户无 manage:view 权限";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 权限检查：manage:edit
                const bool can_edit = std::find(permissions.begin(), permissions.end(), "manage:edit") != permissions.end();

                // 查询所有用户及其权限列表
                pqxx::work txn{ conn };
                const auto rows = txn.exec(
                    "SELECT u.id, u.username, u.key_enabled, u.enabled, u.password_hash, p.name "
                    "FROM users u "
                    "LEFT JOIN user_permissions up ON up.user_id = u.id "
                    "LEFT JOIN permissions p ON p.id = up.permission_id "
                    "ORDER BY u.id, up.permission_id"
                );

                nlohmann::json users = nlohmann::json::array();
                int current_id = 0;
                nlohmann::json current_user;
                for (const auto& row : rows)
                {
                    const int user_id = row["id"].as<int>();
                    if (user_id != current_id)
                    {
                        if (current_id != 0)
                            users.push_back(std::move(current_user));
                        current_id = user_id;
                        current_user = nlohmann::json{
                            {"id",           user_id},
                            {"username",     row["username"].is_null()
                                               ? nlohmann::json(nullptr)
                                               : nlohmann::json(row["username"].as<std::string>())},
                            {"key_enabled",  row["key_enabled"].as<bool>()},
                            {"enabled",      row["enabled"].as<bool>()},
                            {"has_password", !row["password_hash"].is_null()},
                            {"permissions",  nlohmann::json::array()}
                        };
                    }
                    if (!row["name"].is_null())
                        current_user["permissions"].push_back(row["name"].as<std::string>());
                }
                if (current_id != 0)
                    users.push_back(std::move(current_user));

                // 全部权限名（供前端编辑时勾选）
                nlohmann::json all_permissions = nlohmann::json::array();
                const auto perm_rows = txn.exec("SELECT name FROM permissions ORDER BY id");
                for (const auto& row : perm_rows)
                {
                    all_permissions.push_back(row["name"].as<std::string>());
                }
                txn.commit();

                spdlog::debug("{}成功。", ACTION);

                json["users"]           = std::move(users);
                json["all_permissions"] = std::move(all_permissions);
                json["can_edit"]        = can_edit;
                res.set_content(json.dump(), "application/json");
            }
        );
    }

    void handle_manage_update_user(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "用户信息更新";
        spdlog::info("收到 {} 请求。", ACTION);

        nlohmann::json json;

        with_db(res, allowed, ACTION,
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                // Session 验证
                std::string token;  // 提取 Bearer token
                if (req.has_header("Authorization"))
                {
                    const auto& auth_hdr = req.get_header_value("Authorization");
                    constexpr std::string_view PREFIX = "Bearer ";
                    if (auth_hdr.size() > PREFIX.size()
                    &&  auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                        token = auth_hdr.substr(PREFIX.size());
                }

                const auto session = auth::validate_session(conn, token);
                if (!session)
                {
                    spdlog::info("{}失败：未登录或会话已过期。", ACTION);

                    res.status = 401;

                    json["error"] = "未登录或会话已过期";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 权限检查：manage:edit
                const auto& permissions = session->permissions;
                if (std::find(permissions.begin(), permissions.end(), "manage:edit") == permissions.end())
                {
                    spdlog::info("{}失败：用户 {} 无 manage:edit 权限。", ACTION, session->user_id);

                    res.status = 403;

                    json["error"] = "当前用户无 manage:edit 权限";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 解析请求体
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded() || !body.contains("id") || !body["id"].is_number_integer())
                {
                    spdlog::info("{}失败：无效的 JSON。", ACTION);

                    res.status = 400;

                    json["error"] = "无效的 JSON";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                const int user_id = body["id"].get<int>();
                const bool has_username  = body.contains("username");
                const bool has_key_state = body.contains("key_enabled");
                const bool has_enabled   = body.contains("enabled");
                const bool has_key       = body.contains("key");
                const bool has_password  = body.contains("password");
                const bool has_perms     = body.contains("permissions");

                std::string username;
                if (has_username)
                {
                    if (!body["username"].is_string())
                    {
                        spdlog::info("{}失败：用户名格式无效。", ACTION);

                        res.status = 400;

                        json["error"] = "用户名格式无效";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                    username = body["username"].get<std::string>();

                    // 按 UTF-8 码点计数，限制最多 10 个字符
                    std::size_t char_count = 0;
                    for (unsigned char c : username)
                    {
                        if ((c & 0xC0) != 0x80)
                            ++char_count;
                    }
                    if (char_count > 10)
                    {
                        spdlog::info("{}失败：用户名过长。", ACTION);

                        res.status = 400;

                        json["error"] = "用户名最多 10 个字符";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                }
                if (has_key_state && !body["key_enabled"].is_boolean())
                {
                    spdlog::info("{}失败：密钥可用状态无效。", ACTION);

                    res.status = 400;

                    json["error"] = "密钥可用状态格式无效";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                if (has_enabled && !body["enabled"].is_boolean())
                {
                    spdlog::info("{}失败：用户可用状态无效。", ACTION);

                    res.status = 400;

                    json["error"] = "用户可用状态格式无效";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                if (has_key && !body["key"].is_string())
                {
                    spdlog::info("{}失败：密钥格式无效。", ACTION);

                    res.status = 400;

                    json["error"] = "密钥格式无效";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                if (has_password && !body["password"].is_string())
                {
                    spdlog::info("{}失败：密码格式无效。", ACTION);

                    res.status = 400;

                    json["error"] = "密码格式无效";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                if (has_perms && !body["permissions"].is_array())
                {
                    spdlog::info("{}失败：权限列表格式无效。", ACTION);

                    res.status = 400;

                    json["error"] = "权限列表格式无效";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                pqxx::work txn{ conn };

                // 查询当前用户信息（缺失时拒绝）
                const auto user_rows = txn.exec(
                    "SELECT username, key_enabled, enabled, password_hash, key_hash FROM users WHERE id = $1",
                    pqxx::params{ user_id }
                );
                if (user_rows.empty())
                {
                    spdlog::info("{}失败：用户不存在。", ACTION);

                    res.status = 404;

                    json["error"] = "用户不存在";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                const auto& user_row = user_rows[0];

                // 用户名唯一性检查（提供且非空时）
                if (has_username && !username.empty())
                {
                    const auto dup_rows = txn.exec(
                        "SELECT id FROM users WHERE username = $1 AND id <> $2",
                        pqxx::params{ username, user_id }
                    );
                    if (!dup_rows.empty())
                    {
                        spdlog::info("{}失败：用户名重复。", ACTION);

                        res.status = 400;

                        json["error"] = "用户名已存在";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                }

                // 密钥哈希（仅当提供新密钥时；固定盐，与密钥登录一致）
                std::optional<std::string> key_hash;
                if (has_key)
                {
                    const std::string key = body["key"].get<std::string>();
                    if (!key.empty())
                    {
                        key_hash = crypto::Argon2id::hash_with_fixed_salt(key);
                        if (!key_hash)
                        {
                            spdlog::error("{}失败：密钥哈希失败（用户 {}）。", ACTION, user_id);

                            res.status = 500;

                            json["error"] = "密钥哈希失败";
                            res.set_content(json.dump(), "application/json");
                            return;
                        }
                    }
                }

                // 密码哈希（仅当提供新密码时；随机盐，与密码登录一致）
                std::optional<std::string> password_hash;
                if (has_password)
                {
                    const std::string password = body["password"].get<std::string>();
                    if (!password.empty())
                    {
                        password_hash = crypto::Argon2id::hash_with_random_salt(password);
                        if (!password_hash)
                        {
                            spdlog::error("{}失败：密码哈希失败（用户 {}）。", ACTION, user_id);

                            res.status = 500;

                            json["error"] = "密码哈希失败";
                            res.set_content(json.dump(), "application/json");
                            return;
                        }
                    }
                }

                // 计算最终字段值（未提供的字段保持原值）
                const std::optional<std::string> final_username = has_username
                                                                ? (username.empty() ? std::nullopt : std::optional<std::string>{ username })
                                                                : (user_row["username"].is_null()
                                                                    ? std::nullopt
                                                                    : std::optional<std::string>{ user_row["username"].as<std::string>() });
                const bool final_key_enabled = has_key_state
                                             ? body["key_enabled"].get<bool>()
                                             : user_row["key_enabled"].as<bool>();
                const bool final_enabled = has_enabled
                                         ? body["enabled"].get<bool>()
                                         : user_row["enabled"].as<bool>();
                const std::optional<std::string> final_password_hash = password_hash.has_value()
                                                                     ? password_hash
                                                                     : (user_row["password_hash"].is_null()
                                                                        ? std::nullopt
                                                                        : std::optional<std::string>{ user_row["password_hash"].as<std::string>() });
                const std::optional<std::string> final_key_hash = key_hash.has_value()
                                                                ? key_hash
                                                                : (user_row["key_hash"].is_null()
                                                                   ? std::nullopt
                                                                   : std::optional<std::string>{ user_row["key_hash"].as<std::string>() });
                txn.exec(
                    "UPDATE users SET username = $1, key_enabled = $2, enabled = $3, password_hash = $4, key_hash = $5 WHERE id = $6",
                    pqxx::params{ final_username, final_key_enabled, final_enabled, final_password_hash, final_key_hash, user_id }
                );

                // 权限列表（提供时整体替换）
                if (has_perms)
                {
                    txn.exec(
                        "DELETE FROM user_permissions WHERE user_id = $1",
                        pqxx::params{ user_id }
                    );
                    for (const auto& perm : body["permissions"])
                    {
                        if (!perm.is_string())
                            continue;

                        const auto perm_rows = txn.exec(
                            "SELECT id FROM permissions WHERE name = $1",
                            pqxx::params{ perm.get<std::string>() }
                        );

                        if (perm_rows.empty())
                            continue;

                        txn.exec(
                            "INSERT INTO user_permissions (user_id, permission_id) VALUES ($1, $2)",
                            pqxx::params{ user_id, perm_rows[0]["id"].as<int>() }
                        );
                    }
                }

                txn.commit();

                spdlog::info("{}成功：用户 {}。", ACTION, user_id);

                json["ok"] = true;
                res.set_content(json.dump(), "application/json");
            }
        );
    }

    void handle_manage_create_user(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "用户创建";
        spdlog::info("收到 {} 请求。", ACTION);

        nlohmann::json json;

        with_db(res, allowed, ACTION,
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                // Session 验证
                std::string token;  // 提取 Bearer token
                if (req.has_header("Authorization"))
                {
                    const auto& auth_hdr = req.get_header_value("Authorization");
                    constexpr std::string_view PREFIX = "Bearer ";
                    if (auth_hdr.size() > PREFIX.size()
                    &&  auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                        token = auth_hdr.substr(PREFIX.size());
                }

                const auto session = auth::validate_session(conn, token);
                if (!session)
                {
                    spdlog::info("{}失败：未登录或会话已过期。", ACTION);

                    res.status = 401;

                    json["error"] = "未登录或会话已过期";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 权限检查：manage:edit
                const auto& permissions = session->permissions;
                if (std::find(permissions.begin(), permissions.end(), "manage:edit") == permissions.end())
                {
                    spdlog::info("{}失败：用户 {} 无 manage:edit 权限。", ACTION, session->user_id);

                    res.status = 403;

                    json["error"] = "当前用户无 manage:edit 权限";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 解析请求体
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("{}失败：无效的 JSON。", ACTION);

                    res.status = 400;

                    json["error"] = "无效的 JSON";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 密钥必填（key_hash 字段 NOT NULL）
                if (!body.contains("key") || !body["key"].is_string() || body["key"].get<std::string>().empty())
                {
                    spdlog::info("{}失败：未设置密钥。", ACTION);

                    res.status = 400;

                    json["error"] = "新用户必须设置密钥";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                const std::string key = body["key"].get<std::string>();

                // 用户名（可选，非空时校验长度与唯一性）
                std::string username;
                if (body.contains("username"))
                {
                    if (!body["username"].is_string())
                    {
                        spdlog::info("{}失败：用户名格式无效。", ACTION);

                        res.status = 400;

                        json["error"] = "用户名格式无效";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                    username = body["username"].get<std::string>();
                    std::size_t char_count = 0;
                    for (unsigned char c : username)
                    {
                        if ((c & 0xC0) != 0x80)
                            ++char_count;
                    }
                    if (char_count > 10)
                    {
                        spdlog::info("{}失败：用户名过长。", ACTION);

                        res.status = 400;

                        json["error"] = "用户名最多 10 个字符";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                }

                // 密钥可用状态（可选，默认启用）
                bool key_enabled = true;
                if (body.contains("key_enabled"))
                {
                    if (!body["key_enabled"].is_boolean())
                    {
                        spdlog::info("{}失败：密钥可用状态无效。", ACTION);

                        res.status = 400;

                        json["error"] = "密钥可用状态格式无效";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                    key_enabled = body["key_enabled"].get<bool>();
                }

                // 用户可用状态（可选，默认启用）
                bool enabled = true;
                if (body.contains("enabled"))
                {
                    if (!body["enabled"].is_boolean())
                    {
                        spdlog::info("{}失败：用户可用状态无效。", ACTION);

                        res.status = 400;

                        json["error"] = "用户可用状态格式无效";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                    enabled = body["enabled"].get<bool>();
                }

                // 密码（可选，非空时随机盐哈希）
                std::optional<std::string> password_hash;
                if (body.contains("password"))
                {
                    if (!body["password"].is_string())
                    {
                        spdlog::info("{}失败：密码格式无效。", ACTION);

                        res.status = 400;

                        json["error"] = "密码格式无效";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }

                    const std::string password = body["password"].get<std::string>();
                    if (!password.empty())
                    {
                        password_hash = crypto::Argon2id::hash_with_random_salt(password);
                        if (!password_hash)
                        {
                            spdlog::error("{}失败：密码哈希失败。", ACTION);

                            res.status = 500;

                            json["error"] = "密码哈希失败";
                            res.set_content(json.dump(), "application/json");
                            return;
                        }
                    }
                }

                // 权限列表（可选）
                nlohmann::json perm_list = body.value("permissions", nlohmann::json::array());
                if (!perm_list.is_array())
                {
                    spdlog::info("{}失败：权限列表格式无效。", ACTION);

                    res.status = 400;

                    json["error"] = "权限列表格式无效";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                pqxx::work txn{ conn };

                // 用户名唯一性
                if (!username.empty())
                {
                    const auto dup_rows = txn.exec(
                        "SELECT id FROM users WHERE username = $1",
                        pqxx::params{ username }
                    );
                    if (!dup_rows.empty())
                    {
                        spdlog::info("{}失败：重复的用户名。", ACTION);

                        res.status = 400;

                        json["error"] = "用户名已存在";
                        res.set_content(json.dump(), "application/json");
                        return;
                    }
                }

                // 密钥哈希（固定盐，与密钥登录一致）
                const auto key_hash = crypto::Argon2id::hash_with_fixed_salt(key);
                if (!key_hash)
                {
                    spdlog::error("{}失败：密钥哈希失败。", ACTION);

                    res.status = 500;

                    json["error"] = "密钥哈希失败";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 密钥唯一性
                const auto key_dup_rows = txn.exec(
                    "SELECT id FROM users WHERE key_hash = $1",
                    pqxx::params{ *key_hash }
                );
                if (!key_dup_rows.empty())
                {
                    spdlog::info("{}失败：重复的密钥。", ACTION);

                    res.status = 400;

                    json["error"] = "密钥已被其他用户使用";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 插入用户
                const std::optional<std::string> username_param = username.empty()
                                                                ? std::nullopt
                                                                : std::optional<std::string>{ username };
                const auto insert_rows = txn.exec(
                    "INSERT INTO users (key_hash, key_enabled, enabled, username, password_hash) "
                    "VALUES ($1, $2, $3, $4, $5) RETURNING id",
                    pqxx::params{
                        *key_hash,
                        key_enabled,
                        enabled,
                        username_param,
                        password_hash
                    }
                );
                const int user_id = insert_rows[0]["id"].as<int>();

                // 权限
                for (const auto& perm : perm_list)
                {
                    if (!perm.is_string())
                        continue;

                    const auto perm_rows = txn.exec(
                        "SELECT id FROM permissions WHERE name = $1",
                        pqxx::params{ perm.get<std::string>() }
                    );

                    if (perm_rows.empty())
                        continue;

                    txn.exec(
                        "INSERT INTO user_permissions (user_id, permission_id) VALUES ($1, $2)",
                        pqxx::params{ user_id, perm_rows[0]["id"].as<int>() }
                    );
                }

                txn.commit();

                spdlog::info("{}成功：用户 {}。", ACTION, user_id);

                json["ok"] = true;
                json["id"] = user_id;
                res.set_content(json.dump(), "application/json");
            }
        );
    }

} // namespace http
