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

                    nlohmann::json err;
                    err["error"] = "无效的 JSON";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 登录频率限制检查
                const auto& ip = req.remote_addr;
                if (auth::is_rate_limited(ip))
                {
                    spdlog::info("密钥登录失败：IP {} 已被限流。", ip);
                    res.status = 429;

                    nlohmann::json err;
                    err["error"] = "登录尝试过于频繁，请稍后再试";
                    res.set_content(err.dump(), "application/json");
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

                // 登录成功清除 IP 限制
                spdlog::info("密钥登录成功（ip={}, user_id={}）。", ip, result->id);
                auth::clear(ip);

                nlohmann::json success;
                success["id"]         = result->id;
                success["username"]   = result->username.has_value()
                                   ? nlohmann::json(*result->username)
                                   : nlohmann::json(nullptr);
                const auto session    = auth::create_session(conn, result->id, result->permissions);
                success["token"]      = session.token;
                success["expires_at"] = session.expires_at;
                res.set_content(success.dump(), "application/json");
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
                spdlog::info("收到密码登录请求（ip={}）。", req.remote_addr);
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::debug("密码登录失败：无效的 JSON。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "无效的 JSON";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 登录频率限制检查
                const auto& ip = req.remote_addr;
                if (auth::is_rate_limited(ip))
                {
                    spdlog::debug("密码登录失败：IP {} 已被限流。", req.remote_addr);
                    res.status = 429;

                    nlohmann::json err;
                    err["error"] = "登录尝试过于频繁，请稍后再试";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 调用登录逻辑
                const auto username = body.value("username", "");
                const auto pwd      = body.value("password", "");
                auto result = auth::login_by_password(conn, username, pwd);

                if (!result)
                {
                    spdlog::debug("密码登录失败（ip={}）：{}", ip, result.error());
                    res.status = 401;
                    auth::record_failure(ip);

                    nlohmann::json err;
                    err["error"] = result.error();
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 登录成功清除 IP 限制
                spdlog::info("密码登录成功（ip={}, user_id={}）。", ip, result->id);
                auth::clear(ip);

                nlohmann::json success;
                success["id"]         = result->id;
                success["username"]   = result->username.has_value()
                                      ? nlohmann::json(*result->username)
                                      : nlohmann::json(nullptr);
                const auto created    = auth::create_session(conn, result->id, result->permissions);
                success["token"]      = created.token;
                success["expires_at"] = created.expires_at;
                res.set_content(success.dump(), "application/json");
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
                spdlog::debug("收到个人权限获取请求。");
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
                    spdlog::debug("用户获取自身权限失败：未登录或会话已过期。");
                    res.status = 401;

                    nlohmann::json err;
                    err["error"] = "未登录或会话已过期";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                spdlog::debug("用户获取自身权限成功（user_id={}）。", session->user_id);

                nlohmann::json success;
                success["permissions"] = session->permissions;
                res.set_content(success.dump(), "application/json");
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
                spdlog::debug("收到个人信息获取请求。");
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
                    spdlog::debug("用户获取自身信息失败：未登录或会话已过期。");
                    res.status = 401;

                    nlohmann::json err;
                    err["error"] = "未登录或会话已过期";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 查询用户信息
                pqxx::work txn{ conn };
                const auto rows = txn.exec(
                    "SELECT username, key_enabled, password_hash FROM users WHERE id = $1",
                    pqxx::params{ session->user_id }
                );
                txn.commit();

                if (rows.empty())
                {
                    res.status = 404;

                    nlohmann::json err;
                    err["error"] = "用户不存在";
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                const auto& row = rows[0];

                spdlog::debug("用户获取自身信息成功（user_id={}）。", session->user_id);

                nlohmann::json success;
                success["id"]           = session->user_id;
                success["username"]     = row["username"].is_null()
                                        ? nlohmann::json(nullptr)
                                        : nlohmann::json(row["username"].as<std::string>());
                success["key_enabled"]  = row["key_enabled"].as<bool>();
                success["has_password"] = !row["password_hash"].is_null();
                res.set_content(success.dump(), "application/json");
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
                spdlog::debug("收到个人信息更新请求。");
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
                    spdlog::debug("更新个人信息失败：未登录或会话已过期。");
                    res.status = 401;

                    nlohmann::json err;
                    err["error"] = "未登录或会话已过期";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 解析请求体（字段均可选，只更新提供的字段；只能修改自己）
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "无效的 JSON";
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                const bool has_username  = body.contains("username");
                const bool has_key_state = body.contains("key_enabled");
                const bool has_password  = body.contains("password");

                // 检查字段格式
                std::string username;
                if (has_username)
                {
                    if (!body["username"].is_string())
                    {
                        res.status = 400;

                        nlohmann::json err;
                        err["error"] = "用户名格式无效";
                        res.set_content(err.dump(), "application/json");
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
                        res.status = 400;

                        nlohmann::json err;
                        err["error"] = "用户名最多 10 个字符";
                        res.set_content(err.dump(), "application/json");
                        return;
                    }
                }
                if (has_key_state && !body["key_enabled"].is_boolean())
                {
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "密钥可用状态格式无效";
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                if (has_password && !body["password"].is_string())
                {
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "密码格式无效";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                pqxx::work txn{ conn };
                const auto rows = txn.exec(
                    "SELECT username, key_enabled, password_hash FROM users WHERE id = $1",
                    pqxx::params{ session->user_id }
                );
                if (rows.empty())
                {
                    res.status = 404;

                    nlohmann::json err;
                    err["error"] = "用户不存在";
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                const auto& row = rows[0];

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

                        nlohmann::json err;
                        err["error"] = "用户名已被占用";
                        res.set_content(err.dump(), "application/json");
                        return;
                    }
                }

                // 密码哈希（仅当提供新密码时）
                std::optional<std::string> password_hash;
                if (has_password)
                {
                    const std::string password = body["password"].get<std::string>();
                    if (!password.empty())
                    {
                        password_hash = crypto::Argon2id::hash_with_random_salt(password);
                        if (!password_hash)
                        {
                            res.status = 500;
                            spdlog::error("修改用户信息失败：密码哈希失败。");

                            nlohmann::json err;
                            err["error"] = "密码哈希失败";
                            res.set_content(err.dump(), "application/json");
                            return;
                        }
                    }
                }

                // 修改用户信息
                const std::optional<std::string> final_username = has_username
                                                                ? (username.empty()
                                                                    ? std::nullopt
                                                                    : std::optional<std::string>{ username })
                                                                : (row["username"].is_null()
                                                                    ? std::nullopt
                                                                    : std::optional<std::string>{ row["username"].as<std::string>() });
                const bool final_key_enabled = has_key_state
                                             ? body["key_enabled"].get<bool>()
                                             : row["key_enabled"].as<bool>();
                const std::optional<std::string> final_password_hash = password_hash.has_value()
                                                                     ? password_hash
                                                                     : (row["password_hash"].is_null()
                                                                        ? std::nullopt
                                                                        : std::optional<std::string>{ row["password_hash"].as<std::string>() });

                txn.exec(
                    "UPDATE users SET username = $1, key_enabled = $2, password_hash = $3 WHERE id = $4",
                    pqxx::params{ final_username, final_key_enabled, final_password_hash, session->user_id }
                );
                txn.commit();

                spdlog::debug("用户更新自身信息成功（user_id={}）。", session->user_id);

                nlohmann::json success;
                success["ok"] = true;
                res.set_content(success.dump(), "application/json");
            }
        );
    }

} // namespace http
