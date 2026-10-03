/**
 * @file http/handlers/auth.cpp
 * @brief 登录认证与当前用户信息 HTTP 路由处理函数实现
 */

#include "http/handlers/auth.h"

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "auth/login.h"
#include "auth/rate_limit.h"
#include "auth/session.h"
#include "crypto/argon2id.h"
#include "db/connection_pool.h"

namespace http
{
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
                spdlog::info("收到密钥登录请求（ip={}）。", req.remote_addr);
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
                auto result = auth::login_by_key(conn, key);

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

} // namespace http
