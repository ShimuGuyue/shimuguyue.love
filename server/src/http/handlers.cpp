/**
 * @file http/handlers.cpp
 * @brief HTTP 路由处理函数实现
 */

#include "http/handlers.h"

#include <algorithm>
#include <ctime>
#include <sstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "about/about_queries.h"
#include "auth/login.h"
#include "auth/rate_limit.h"
#include "auth/session.h"
#include "crypto/argon2id.h"
#include "db/connection_pool.h"
#include "doc/blog_queries.h"
#include "img/image_queries.h"
#include "md/markdown_parser.h"
#include "profile/profile_queries.h"





namespace http
{
    void handle_cors(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        res.set_header("Access-Control-Allow-Origin",  allowed);
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    }

    void handle_login_key(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                // 解析 JSON
                spdlog::debug("收到密钥登录请求（ip={}）。", req.remote_addr);
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::debug("密钥登录失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

                // 登录频率限制检查
                const auto& ip = req.remote_addr;
                if (auth::is_rate_limited(ip))
                {
                    spdlog::info("密钥登录失败：IP {} 已被限流。", ip);
                    res.status = 429;
                    res.set_content(R"({"error":"登录尝试过于频繁，请稍后再试"})", "application/json");
                    return;
                }

                // 调用登录逻辑
                const auto key = body.value("key", "");
                auto    result = auth::login_by_key(conn, key);

                if (!result)
                {
                    spdlog::debug("密钥登录失败（ip={}）：{}", ip, result.error());
                    res.status = 401;
                    auth::record_failure(ip);
                    nlohmann::json err;
                    err["error"] = result.error();
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                auth::clear(ip);
                spdlog::debug("密钥登录成功（ip={}，user_id={}）。", ip, result->id);
                nlohmann::json resp;
                resp["id"] = result->id;
                resp["username"] = result->username.has_value()
                                 ? nlohmann::json(*result->username)
                                 : nlohmann::json(nullptr);
                const auto created = auth::create_session(conn, result->id, result->permissions);
                resp["token"]      = created.token;
                resp["expires_at"] = created.expires_at;
                res.set_content(resp.dump(), "application/json");
            }
        );
    }

    void handle_login_password(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                // 解析 JSON
                spdlog::debug("收到密码登录请求（ip={}）。", req.remote_addr);

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::debug("密码登录失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

                // 登录频率限制检查
                const auto& ip = req.remote_addr;
                if (auth::is_rate_limited(ip))
                {
                    spdlog::debug("密码登录失败：IP {} 已被限流。", ip);
                    res.status = 429;
                    res.set_content(R"({"error":"登录尝试过于频繁，请稍后再试"})", "application/json");
                    return;
                }

                // 调用登录逻辑
                const auto username = body.value("username", "");
                const auto pwd      = body.value("password", "");
                auto       result   = auth::login_by_password(conn, username, pwd);

                if (!result)
                {
                    spdlog::debug("密码登录失败（ip={}，user={}）：{}", ip, username, result.error());
                    res.status = 401;
                    auth::record_failure(ip);
                    nlohmann::json err;
                    err["error"] = result.error();
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                auth::clear(ip);
                spdlog::debug("密码登录成功（ip={}，user={}）。", ip, username);
                nlohmann::json resp;
                resp["id"] = result->id;
                resp["username"] = result->username.has_value()
                                 ? nlohmann::json(*result->username)
                                 : nlohmann::json(nullptr);
                const auto created = auth::create_session(conn, result->id, result->permissions);
                resp["token"]      = created.token;
                resp["expires_at"] = created.expires_at;
                res.set_content(resp.dump(), "application/json");
            }
        );
    }

    void handle_user_permissions(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                // Session 验证：仅返回已登录用户自身的权限
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
                    spdlog::info("获取权限失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                res.set_content(nlohmann::json{{"permissions", session->permissions}}.dump(), "application/json");
            }
        );
    }

    void handle_user_info(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("获取用户信息失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 查询用户信息
                pqxx::work txn{ conn };
                const auto rows = txn.exec(
                    "SELECT username, key_enabled, password_hash FROM users WHERE id = $1",
                    pqxx::params{ session->user_id }
                );
                txn.commit();

                nlohmann::json resp;
                resp["id"] = session->user_id;
                resp["username"] = rows.empty() || rows[0]["username"].is_null()
                    ? nlohmann::json(nullptr)
                    : nlohmann::json(rows[0]["username"].as<std::string>());
                resp["key_enabled"] = rows.empty() ? true : rows[0]["key_enabled"].as<bool>();
                resp["has_password"] = !rows.empty() && !rows[0]["password_hash"].is_null();
                res.set_content(resp.dump(), "application/json");
            }
        );
    }

    void handle_user_update(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("更新个人信息失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 解析请求体（字段均可选，只更新提供的字段；只能修改自己）
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }
                const bool has_username  = body.contains("username");
                const bool has_key_state = body.contains("key_enabled");
                const bool has_password  = body.contains("password");

                std::string username;
                if (has_username)
                {
                    if (!body["username"].is_string())
                    {
                        res.status = 400;
                        res.set_content(R"({"error":"用户名格式无效"})", "application/json");
                        return;
                    }
                    username = body["username"].get<std::string>();
                    std::size_t char_count = 0;
                    for (unsigned char c : username)
                    {
                        if ((c & 0xC0) != 0x80)
                        {
                            ++char_count;
                        }
                    }
                    if (char_count > 10)
                    {
                        res.status = 400;
                        res.set_content(R"({"error":"用户名最多 10 个字符"})", "application/json");
                        return;
                    }
                }
                if (has_key_state && !body["key_enabled"].is_boolean())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"密钥可用状态格式无效"})", "application/json");
                    return;
                }
                if (has_password && !body["password"].is_string())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"密码格式无效"})", "application/json");
                    return;
                }

                pqxx::work txn{ conn };
                const auto user_rows = txn.exec(
                    "SELECT username, key_enabled, password_hash FROM users WHERE id = $1",
                    pqxx::params{ session->user_id }
                );
                if (user_rows.empty())
                {
                    res.status = 404;
                    res.set_content(R"({"error":"用户不存在"})", "application/json");
                    return;
                }
                const auto& user_row = user_rows[0];

                // 用户名唯一性
                if (has_username && !username.empty())
                {
                    const auto dup_rows = txn.exec(
                        "SELECT id FROM users WHERE username = $1 AND id <> $2",
                        pqxx::params{ username, session->user_id }
                    );
                    if (!dup_rows.empty())
                    {
                        res.status = 400;
                        res.set_content(R"({"error":"用户名已存在"})", "application/json");
                        return;
                    }
                }

                // 密码哈希（仅当提供新密码时；随机盐）
                std::optional<std::string> password_hash;
                if (has_password)
                {
                    const std::string password = body["password"].get<std::string>();
                    if (!password.empty())
                    {
                        password_hash = crypto::Argon2id::hash_with_random_salt(password);
                        if (!password_hash)
                        {
                            spdlog::error("更新个人信息失败：密码哈希失败（用户 {}）。", session->user_id);
                            res.status = 500;
                            res.set_content(R"({"error":"密码哈希失败"})", "application/json");
                            return;
                        }
                    }
                }

                const std::optional<std::string> final_username =
                    has_username
                    ? (username.empty() ? std::nullopt : std::optional<std::string>{ username })
                    : (user_row["username"].is_null()
                       ? std::nullopt
                       : std::optional<std::string>{ user_row["username"].as<std::string>() });
                const bool final_key_enabled =
                    has_key_state ? body["key_enabled"].get<bool>() : user_row["key_enabled"].as<bool>();
                const std::optional<std::string> final_password_hash =
                    password_hash.has_value()
                    ? password_hash
                    : (user_row["password_hash"].is_null()
                       ? std::nullopt
                       : std::optional<std::string>{ user_row["password_hash"].as<std::string>() });

                txn.exec(
                    "UPDATE users SET username = $1, key_enabled = $2, password_hash = $3 WHERE id = $4",
                    pqxx::params{ final_username, final_key_enabled, final_password_hash, session->user_id }
                );
                txn.commit();
                spdlog::info("更新个人信息成功：用户 {}。", session->user_id);
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

    void handle_manage_users(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("获取用户列表失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 权限检查：仅 manage 权限用户可查看用户列表
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage") == perms.end())
                {
                    spdlog::info("获取用户列表失败：用户 {} 无 manage 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage 权限"})", "application/json");
                    return;
                }

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
                        {
                            users.push_back(std::move(current_user));
                        }
                        current_id = user_id;
                        current_user = nlohmann::json{
                            {"id", user_id},
                            {"username", row["username"].is_null()
                                        ? nlohmann::json(nullptr)
                                        : nlohmann::json(row["username"].as<std::string>())},
                            {"key_enabled", row["key_enabled"].as<bool>()},
                            {"enabled", row["enabled"].as<bool>()},
                            {"has_password", !row["password_hash"].is_null()},
                            {"permissions", nlohmann::json::array()}
                        };
                    }
                    if (!row["name"].is_null())
                    {
                        current_user["permissions"].push_back(row["name"].as<std::string>());
                    }
                }
                if (current_id != 0)
                {
                    users.push_back(std::move(current_user));
                }

                // 全部权限名（供前端编辑时勾选）
                nlohmann::json all_permissions = nlohmann::json::array();
                const auto perm_rows = txn.exec("SELECT name FROM permissions ORDER BY id");
                for (const auto& row : perm_rows)
                {
                    all_permissions.push_back(row["name"].as<std::string>());
                }
                txn.commit();

                res.set_content(
                    nlohmann::json{
                        {"users", std::move(users)},
                        {"all_permissions", std::move(all_permissions)}
                    }.dump(),
                    "application/json"
                );
            }
        );
    }

    void handle_manage_update_user(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("更新用户失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 权限检查：仅 manage 权限用户可更新用户
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage") == perms.end())
                {
                    spdlog::info("更新用户失败：用户 {} 无 manage 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage 权限"})", "application/json");
                    return;
                }

                // 解析请求体
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded() || !body.contains("id") || !body["id"].is_number_integer())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
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
                        res.status = 400;
                        res.set_content(R"({"error":"用户名格式无效"})", "application/json");
                        return;
                    }
                    username = body["username"].get<std::string>();

                    // 按 UTF-8 码点计数，限制最多 10 个字符
                    std::size_t char_count = 0;
                    for (unsigned char c : username)
                    {
                        if ((c & 0xC0) != 0x80)
                        {
                            ++char_count;
                        }
                    }
                    if (char_count > 10)
                    {
                        res.status = 400;
                        res.set_content(R"({"error":"用户名最多 10 个字符"})", "application/json");
                        return;
                    }
                }
                if (has_key_state && !body["key_enabled"].is_boolean())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"密钥可用状态格式无效"})", "application/json");
                    return;
                }
                if (has_enabled && !body["enabled"].is_boolean())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"用户可用状态格式无效"})", "application/json");
                    return;
                }
                if (has_key && !body["key"].is_string())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"密钥格式无效"})", "application/json");
                    return;
                }
                if (has_password && !body["password"].is_string())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"密码格式无效"})", "application/json");
                    return;
                }
                if (has_perms && !body["permissions"].is_array())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"权限列表格式无效"})", "application/json");
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
                    res.status = 404;
                    res.set_content(R"({"error":"用户不存在"})", "application/json");
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
                        res.status = 400;
                        res.set_content(R"({"error":"用户名已存在"})", "application/json");
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
                            spdlog::error("更新用户失败：密钥哈希失败（用户 {}）。", user_id);
                            res.status = 500;
                            res.set_content(R"({"error":"密钥哈希失败"})", "application/json");
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
                            spdlog::error("更新用户失败：密码哈希失败（用户 {}）。", user_id);
                            res.status = 500;
                            res.set_content(R"({"error":"密码哈希失败"})", "application/json");
                            return;
                        }
                    }
                }

                // 计算最终字段值（未提供的字段保持原值）
                const std::optional<std::string> final_username =
                    has_username
                    ? (username.empty() ? std::nullopt : std::optional<std::string>{ username })
                    : (user_row["username"].is_null()
                       ? std::nullopt
                       : std::optional<std::string>{ user_row["username"].as<std::string>() });
                const bool final_key_enabled =
                    has_key_state ? body["key_enabled"].get<bool>() : user_row["key_enabled"].as<bool>();
                const bool final_enabled =
                    has_enabled ? body["enabled"].get<bool>() : user_row["enabled"].as<bool>();
                const std::optional<std::string> final_password_hash =
                    password_hash.has_value()
                    ? password_hash
                    : (user_row["password_hash"].is_null()
                       ? std::nullopt
                       : std::optional<std::string>{ user_row["password_hash"].as<std::string>() });
                const std::optional<std::string> final_key_hash =
                    key_hash.has_value()
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
                        {
                            continue;
                        }
                        const auto perm_rows = txn.exec(
                            "SELECT id FROM permissions WHERE name = $1",
                            pqxx::params{ perm.get<std::string>() }
                        );
                        if (perm_rows.empty())
                        {
                            continue;
                        }
                        txn.exec(
                            "INSERT INTO user_permissions (user_id, permission_id) VALUES ($1, $2)",
                            pqxx::params{ user_id, perm_rows[0]["id"].as<int>() }
                        );
                    }
                }

                txn.commit();
                spdlog::info("更新用户成功：用户 {}。", user_id);
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

    void handle_manage_create_user(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("创建用户失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 权限检查：仅 manage 权限用户可创建用户
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage") == perms.end())
                {
                    spdlog::info("创建用户失败：用户 {} 无 manage 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage 权限"})", "application/json");
                    return;
                }

                // 解析请求体
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

                // 密钥必填（key_hash 字段 NOT NULL）
                if (!body.contains("key") || !body["key"].is_string()
                ||  body["key"].get<std::string>().empty())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"新用户必须设置密钥"})", "application/json");
                    return;
                }
                const std::string key = body["key"].get<std::string>();

                // 用户名（可选，非空时校验长度与唯一性）
                std::string username;
                if (body.contains("username"))
                {
                    if (!body["username"].is_string())
                    {
                        res.status = 400;
                        res.set_content(R"({"error":"用户名格式无效"})", "application/json");
                        return;
                    }
                    username = body["username"].get<std::string>();
                    std::size_t char_count = 0;
                    for (unsigned char c : username)
                    {
                        if ((c & 0xC0) != 0x80)
                        {
                            ++char_count;
                        }
                    }
                    if (char_count > 10)
                    {
                        res.status = 400;
                        res.set_content(R"({"error":"用户名最多 10 个字符"})", "application/json");
                        return;
                    }
                }

                // 密钥可用状态（可选，默认启用）
                bool key_enabled = true;
                if (body.contains("key_enabled"))
                {
                    if (!body["key_enabled"].is_boolean())
                    {
                        res.status = 400;
                        res.set_content(R"({"error":"密钥可用状态格式无效"})", "application/json");
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
                        res.status = 400;
                        res.set_content(R"({"error":"用户可用状态格式无效"})", "application/json");
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
                        res.status = 400;
                        res.set_content(R"({"error":"密码格式无效"})", "application/json");
                        return;
                    }
                    const std::string password = body["password"].get<std::string>();
                    if (!password.empty())
                    {
                        password_hash = crypto::Argon2id::hash_with_random_salt(password);
                        if (!password_hash)
                        {
                            spdlog::error("创建用户失败：密码哈希失败。");
                            res.status = 500;
                            res.set_content(R"({"error":"密码哈希失败"})", "application/json");
                            return;
                        }
                    }
                }

                // 权限列表（可选）
                nlohmann::json perm_list = body.value("permissions", nlohmann::json::array());
                if (!perm_list.is_array())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"权限列表格式无效"})", "application/json");
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
                        res.status = 400;
                        res.set_content(R"({"error":"用户名已存在"})", "application/json");
                        return;
                    }
                }

                // 密钥哈希（固定盐，与密钥登录一致）
                const auto key_hash = crypto::Argon2id::hash_with_fixed_salt(key);
                if (!key_hash)
                {
                    spdlog::error("创建用户失败：密钥哈希失败。");
                    res.status = 500;
                    res.set_content(R"({"error":"密钥哈希失败"})", "application/json");
                    return;
                }

                // 密钥唯一性
                const auto key_dup_rows = txn.exec(
                    "SELECT id FROM users WHERE key_hash = $1",
                    pqxx::params{ *key_hash }
                );
                if (!key_dup_rows.empty())
                {
                    res.status = 400;
                    res.set_content(R"({"error":"密钥已被其他用户使用"})", "application/json");
                    return;
                }

                // 插入用户
                const std::optional<std::string> username_param =
                    username.empty() ? std::nullopt : std::optional<std::string>{ username };
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
                    {
                        continue;
                    }
                    const auto perm_rows = txn.exec(
                        "SELECT id FROM permissions WHERE name = $1",
                        pqxx::params{ perm.get<std::string>() }
                    );
                    if (perm_rows.empty())
                    {
                        continue;
                    }
                    txn.exec(
                        "INSERT INTO user_permissions (user_id, permission_id) VALUES ($1, $2)",
                        pqxx::params{ user_id, perm_rows[0]["id"].as<int>() }
                    );
                }

                txn.commit();
                spdlog::info("创建用户成功：用户 {}。", user_id);
                res.set_content(nlohmann::json{{"ok", true}, {"id", user_id}}.dump(), "application/json");
            }
        );
    }

    void handle_get_categories(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                auto categories = doc::get_categories(conn);
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& c : categories)
                {
                    nlohmann::json item;
                    item["id"]   = c.id;
                    item["name"] = c.name;
                    arr.push_back(std::move(item));
                }
                res.set_content(arr.dump(), "application/json");
            }
        );
    }

    void handle_get_tags(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                std::vector<int> category_ids;
                if (req.has_param("category_ids"))
                {
                    const auto raw = req.get_param_value("category_ids");
                    std::istringstream iss{ raw };
                    std::string token;
                    while (std::getline(iss, token, ','))
                    {
                        if (!token.empty())
                            category_ids.push_back(std::stoi(token));
                    }
                }

                auto tags = doc::get_tags(conn, category_ids);
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& t : tags)
                {
                    nlohmann::json item;
                    item["id"]          = t.id;
                    item["name"]        = t.name; 
                    item["category_id"] = t.category_id;
                    arr.push_back(std::move(item));
                }
                res.set_content(arr.dump(), "application/json");
            }
        );
    }

    void handle_get_blogs(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                doc::BlogQuery query;

                if (req.has_param("category_ids"))
                {
                    const auto raw = req.get_param_value("category_ids");
                    std::istringstream iss{ raw };
                    std::string token;
                    while (std::getline(iss, token, ','))
                    {
                        if (!token.empty())
                            query.category_ids.push_back(std::stoi(token));
                    }
                }

                if (req.has_param("tag_ids"))
                {
                    const auto raw = req.get_param_value("tag_ids");
                    std::istringstream iss{ raw };
                    std::string token;
                    while (std::getline(iss, token, ','))
                    {
                        if (!token.empty())
                            query.tag_ids.push_back(std::stoi(token));
                    }
                }

                if (req.has_param("q"))
                    query.search = req.get_param_value("q");

                auto blogs = doc::get_blogs(conn, query);
                nlohmann::json arr = nlohmann::json::array();
                for (const auto& b : blogs)
                {
                    nlohmann::json item;
                    item["id"]          = b.id;
                    item["title"]       = b.title;
                    item["description"] = b.description.has_value()
                                        ? nlohmann::json(*b.description)
                                        : nlohmann::json(nullptr);
                    item["update_time"] = b.update_time;
                    item["category"]    = b.category.has_value()
                                        ? nlohmann::json(*b.category)
                                        : nlohmann::json(nullptr);
                    item["tags"]        = b.tags;
                    item["file_path"]   = b.file_path.has_value()
                                        ? nlohmann::json(*b.file_path)
                                        : nlohmann::json(nullptr);
                    arr.push_back(std::move(item));
                }
                res.set_content(arr.dump(), "application/json");
            }
        );
    }

    void handle_get_blog(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                if (!req.has_param("file_path"))
                {
                    spdlog::error("获取博客失败：缺少 file_path 参数。");
                    res.status = 400;
                    res.set_content(R"({"error":"缺少 file_path 参数"})", "application/json");
                    return;
                }

                const auto fp = req.get_param_value("file_path");
                spdlog::debug("正在获取博客：{}", fp);
                auto blog = doc::get_blog_by_file_path(conn, fp);
                if (!blog)
                {
                    spdlog::error("获取博客失败：{} 不存在。", fp);
                    res.status = 404;
                    res.set_content(R"({"error":"博客不存在"})", "application/json");
                    return;
                }

                nlohmann::json item;
                item["id"]          = blog->id;
                item["title"]       = blog->title;
                item["description"] = blog->description.has_value()
                                    ? nlohmann::json(*blog->description)
                                    : nlohmann::json(nullptr);
                item["content"]     = blog->content.has_value()
                                    ? nlohmann::json(*blog->content)
                                    : nlohmann::json(nullptr);
                item["update_time"] = blog->update_time;
                item["category"]    = blog->category.has_value()
                                    ? nlohmann::json(*blog->category)
                                    : nlohmann::json(nullptr);
                item["file_path"]   = blog->file_path.has_value()
                                    ? nlohmann::json(*blog->file_path)
                                    : nlohmann::json(nullptr);
                item["tags"]        = blog->tags;
                res.set_content(item.dump(), "application/json");
            }
        );
    }

    void handle_blog_parse(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        res.set_header("Access-Control-Allow-Origin", allowed);
        res.set_header("Content-Type", "application/json");
        auto result = md::parse_frontmatter(req.body);
        res.set_content(result.dump(), "application/json");
    }

    void handle_get_images(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");
                res.set_content(img::get_all_images(conn).dump(), "application/json");
            }
        );
    }

    void handle_save_image(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("保存图片元数据失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "edit") == perms.end()) {
                    spdlog::info("保存图片元数据失败：用户 {} 无 edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                    return;
                }

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded()) {
                    spdlog::error("保存图片元数据失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

                const auto err = img::save_image(
                    conn,
                    body.value("path", ""),
                    body.value("description", ""),
                    body.value("scale", 1.0),
                    body.value("rotation", 0.0),
                    body.value("pos_x", 50.0),
                    body.value("pos_y", 50.0),
                    body.value("z", 0)
                );
                if (err.has_value())
                {
                    spdlog::error("保存图片元数据失败：{}", *err);
                    res.status = 500;
                    nlohmann::json j;
                    j["error"] = *err;
                    res.set_content(j.dump(), "application/json");
                    return;
                }
                spdlog::info("图片元数据保存成功：{}", body.value("path", ""));
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

    void handle_upload_image(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                if (!req.form.has_file("file"))
                {
                    spdlog::error("上传图片失败：未选择文件。");
                    res.status = 400;
                    res.set_content(R"({"error":"未选择文件"})", "application/json");
                    return;
                }
                const auto file = req.form.get_file("file");

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
                    spdlog::info("上传图片失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "edit") == perms.end())
                {
                    spdlog::info("上传图片失败：用户 {} 无 edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                    return;
                }

                auto [err, result] = img::upload_image(conn, file.filename, file.content);
                if (err.has_value()) {
                    spdlog::error("上传图片失败：{}", *err);
                    res.status = 500;
                    nlohmann::json j;
                    j["error"] = *err;
                    res.set_content(j.dump(), "application/json");
                    return;
                }
                spdlog::info("图片上传成功：{}。", file.filename);
                res.set_content(result.dump(), "application/json");
            }
        );
    }

    void handle_delete_image(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("删除图片失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "edit") == perms.end())
                {
                    spdlog::info("删除图片失败：用户 {} 无 edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                    return;
                }

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::error("删除图片失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

                const auto path = body.value("path", "");
                const auto err = img::delete_image(conn, path);
                if (err.has_value())
                {
                    spdlog::error("删除图片失败：{}", *err);
                    res.status = 500;
                    nlohmann::json j;
                    j["error"] = *err;
                    res.set_content(j.dump(), "application/json");
                    return;
                }
                spdlog::info("图片删除成功：{}。", path);
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

    void handle_save_blog(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("保存博客失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

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
                    spdlog::info("保存博客失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 权限检查
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "create") == perms.end())
                {
                    spdlog::info("保存博客失败：用户 {} 无 create 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 create 权限"})", "application/json");
                    return;
                }

                const auto title       = body.value("title", "");
                const auto description = body.value("description", "");
                const auto category    = body.value("category", "");
                const auto content     = body.value("content", "");
                const auto pathCat     = body.value("file_path_category", "");
                const auto pathName    = body.value("file_path_name", "");
                const auto tagsJson    = body.value("tags", nlohmann::json::array());

                if (title.empty() || description.empty() || category.empty()
                ||  pathCat.empty() || pathName.empty() || content.empty())
                {
                    spdlog::info("保存博客失败：缺少必填字段。");
                    res.status = 400;
                    res.set_content(R"({"error":"所有字段均为必填"})", "application/json");
                    return;
                }

                std::vector<std::string> tagList;
                if (tagsJson.is_array())
                {
                    for (const auto &t : tagsJson)
                    {
                        if (t.is_string())
                            tagList.push_back(t.template get<std::string>());
                    }
                }

                std::time_t now{ std::time(nullptr) };
                char buf[16];
                std::strftime(buf, sizeof buf, "%Y-%m-%d", std::localtime(&now));

                const auto err = doc::save_blog(
                    conn, title, description, category, tagList,
                    pathCat, pathName,
                    content, buf
                );

                if (err)
                {
                    spdlog::error("保存博客失败：{}", *err);
                    res.status = 500;
                    nlohmann::json j;
                    j["error"] = *err;
                    res.set_content(j.dump(), "application/json");
                    return;
                }

                spdlog::info("博客保存成功：{}/{}。", pathCat, pathName);
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

    void handle_update_blog(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("更新博客失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

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
                    spdlog::info("更新博客失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 权限检查
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "edit") == perms.end())
                {
                    spdlog::info("更新博客失败：用户 {} 无 edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                    return;
                }

                const auto title         = body.value("title", "");
                const auto description   = body.value("description", "");
                const auto category      = body.value("category", "");
                const auto content       = body.value("content", "");
                const auto pathCat       = body.value("file_path_category", "");
                const auto pathName      = body.value("file_path_name", "");
                const auto old_file_path = body.value("old_file_path", "");
                const auto tagsJson      = body.value("tags", nlohmann::json::array());

                if (title.empty() || description.empty() || category.empty()
                ||  pathCat.empty() || pathName.empty() || old_file_path.empty() || content.empty())
                {
                    spdlog::info("更新博客失败：缺少必填字段。");
                    res.status = 400;
                    res.set_content(R"({"error":"所有字段均为必填"})", "application/json");
                    return;
                }

                std::vector<std::string> tagList;
                if (tagsJson.is_array())
                {
                    for (const auto &t : tagsJson)
                    {
                        if (t.is_string())
                            tagList.push_back(t.template get<std::string>());
                    }
                }

                std::time_t now{ std::time(nullptr) };
                char buf[16];
                std::strftime(buf, sizeof buf, "%Y-%m-%d", std::localtime(&now));

                const auto err = doc::update_blog(
                    conn, title, description, category, tagList,
                    old_file_path, pathCat, pathName, content, buf
                );

                if (err)
                {
                    spdlog::error("更新博客失败：{}", *err);
                    res.status = 500;
                    nlohmann::json j;
                    j["error"] = *err;
                    res.set_content(j.dump(), "application/json");
                    return;
                }

                spdlog::info("博客更新成功：{}/{}。", pathCat, pathName);
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

    void handle_delete_blog(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("删除博客失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 权限检查
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "drop") == perms.end())
                {
                    spdlog::info("删除博客失败：用户 {} 无 drop 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 drop 权限"})", "application/json");
                    return;
                }

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::error("删除博客失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

                const auto file_path = body.value("file_path", "");
                if (file_path.empty())
                {
                    spdlog::error("删除博客失败：缺少 file_path 参数。");
                    res.status = 400;
                    res.set_content(R"({"error":"缺少 file_path 参数"})", "application/json");
                    return;
                }

                const auto err = doc::delete_blog(conn, file_path);
                if (err)
                {
                    spdlog::error("删除博客失败：{}", *err);
                    res.status = 500;
                    nlohmann::json j;
                    j["error"] = *err;
                    res.set_content(j.dump(), "application/json");
                    return;
                }

                spdlog::info("博客删除成功：{}。", file_path);
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

    void handle_get_about(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");
                res.set_content(nlohmann::json{{"content", about::get_about(conn)}}.dump(), "application/json");
            }
        );
    }

    void handle_get_profile(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");
                res.set_content(profile::get_profile(conn).dump(), "application/json");
            }
        );
    }

    void handle_save_profile(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
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
                    spdlog::info("更新个人简介失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "edit") == perms.end())
                {
                    spdlog::info("更新个人简介失败：用户 {} 无 edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                    return;
                }

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::error("更新个人简介失败：无效的 JSON。");
                    res.status = 400;
                    res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                    return;
                }

                const auto err = profile::update_profile(
                    conn,
                    body.value("title", ""),
                    body.value("subtitle", ""),
                    body.value("bio", "")
                );
                if (err.has_value())
                {
                    spdlog::error("更新个人简介失败：{}", *err);
                    res.status = 500;
                    nlohmann::json j;
                    j["error"] = *err;
                    res.set_content(j.dump(), "application/json");
                    return;
                }
                spdlog::info("个人简介更新成功。");
                res.set_content(R"({"ok":true})", "application/json");
            }
        );
    }

} // namespace http
