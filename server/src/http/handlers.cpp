/**
 * @file http/handlers.cpp
 * @brief HTTP 路由处理函数实现
 */

#include "http/handlers.h"

#include <algorithm>
#include <charconv>
#include <ctime>
#include <expected>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "about/about_queries.h"
#include "auth/login.h"
#include "auth/rate_limit.h"
#include "auth/session.h"
#include "config/env.h"
#include "crypto/argon2id.h"
#include "db/connection_pool.h"
#include "doc/blog_queries.h"
#include "export/export_data.h"
#include "img/image_queries.h"
#include "md/markdown_parser.h"
#include "profile/profile_queries.h"

namespace
{
    /**
     * @brief 将逗号分隔的 id 列表归一化为数字升序字符串（用于缓存键）。
     * @param raw 原始参数值，例如 "3,1,2"。
     * @return 归一化后的字符串，例如 "1,2,3"；含无效项或为空时返回原值。
     */
    auto normalize_id_list(std::string_view raw) -> std::string
    {
        std::vector<int> ids;
        std::istringstream iss{ std::string{ raw } };
        std::string token;
        while (std::getline(iss, token, ','))
        {
            if (token.empty())
                continue;

            int value{ 0 };
            const auto [ptr, ec] = std::from_chars(
                token.data(),
                token.data() + token.size(),
                value
            );
            if (ec != std::errc{} || ptr != token.data() + token.size())
                return std::string{ raw };

            ids.push_back(value);
        }

        if (ids.empty())
            return std::string{ raw };

        std::sort(ids.begin(), ids.end());
        std::ostringstream out;
        for (std::size_t i{ 0 }; i < ids.size(); ++i)
        {
            if (i != 0)
                out << ',';
            out << ids[i];
        }
        return out.str();
    }

    /**
     * @brief 按 RFC 5987 对 UTF-8 字符串做百分号编码（用于 Content-Disposition filename*）。
     * @param value 原始字符串。
     * @return 编码后的 ASCII 字符串。
     */
    auto percent_encode(std::string_view value) -> std::string
    {
        constexpr char HEX[] = "0123456789ABCDEF";
        std::string out;
        out.reserve(value.size());
        for (const unsigned char c : value)
        {
            const bool unreserved = (c >= 'A' && c <= 'Z')
                                 || (c >= 'a' && c <= 'z')
                                 || (c >= '0' && c <= '9')
                                 || c == '-' || c == '_' || c == '.' || c == '~';
            if (unreserved)
            {
                out.push_back(static_cast<char>(c));
            }
            else
            {
                out.push_back('%');
                out.push_back(HEX[c >> 4]);
                out.push_back(HEX[c & 0x0F]);
            }
        }
        return out;
    }

} // namespace





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

                // 权限检查：仅 manage:view 权限用户可查看用户列表
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage:view") == perms.end())
                {
                    spdlog::info("获取用户列表失败：用户 {} 无 manage:view 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage:view 权限"})", "application/json");
                    return;
                }
                // 是否可编辑（保存 / 新建用户）
                const bool can_edit =
                    std::find(perms.begin(), perms.end(), "manage:edit") != perms.end();

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
                        {"all_permissions", std::move(all_permissions)},
                        {"can_edit", can_edit}
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

                // 权限检查：仅 manage:edit 权限用户可更新用户
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage:edit") == perms.end())
                {
                    spdlog::info("更新用户失败：用户 {} 无 manage:edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage:edit 权限"})", "application/json");
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

                // 权限检查：仅 manage:edit 权限用户可创建用户
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage:edit") == perms.end())
                {
                    spdlog::info("创建用户失败：用户 {} 无 manage:edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage:edit 权限"})", "application/json");
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

    void handle_manage_download(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/zip");

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
                    spdlog::info("数据下载失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 导出范围：blogs = 博客/分类/标签，users = 用户/权限/用户权限关联
                const auto scope = req.get_param_value("scope");
                if (scope != "blogs" && scope != "users")
                {
                    spdlog::info("数据下载失败：无效的导出范围 {}", scope);
                    res.status = 400;
                    res.set_content(R"({"error":"无效的导出范围"})", "application/json");
                    return;
                }

                // 权限检查：数据下载需要 manage:download 权限
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage:download") == perms.end())
                {
                    spdlog::info("数据下载失败：用户 {} 无 manage:download 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage:download 权限"})", "application/json");
                    return;
                }

                std::expected<std::string, std::string> zip;
                if (scope == "blogs")
                {
                    zip = export_data::build_blogs_export_zip(conn);
                }
                else
                {
                    zip = export_data::build_users_export_zip(conn);
                }
                if (!zip)
                {
                    spdlog::error("数据下载失败：{}", zip.error());
                    res.status = 500;
                    res.set_content(
                        nlohmann::json{{"error", zip.error()}}.dump(),
                        "application/json"
                    );
                    return;
                }

                // 附件文件名带上导出日期
                std::time_t now{ std::time(nullptr) };
                char date_buf[16];
                std::strftime(date_buf, sizeof date_buf, "%Y%m%d", std::localtime(&now));
                res.set_header(
                    "Content-Disposition",
                    "attachment; filename=\"data-" + scope + "-" + std::string{ date_buf } + ".zip\""
                );
                res.set_content(zip->data(), zip->size(), "application/zip");
                spdlog::info("数据下载成功。");
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
                if (std::find(perms.begin(), perms.end(), "photo_wall:edit") == perms.end()) {
                    spdlog::info("保存图片元数据失败：用户 {} 无 photo_wall:edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 photo_wall:edit 权限"})", "application/json");
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
                if (std::find(perms.begin(), perms.end(), "photo_wall:upload") == perms.end())
                {
                    spdlog::info("上传图片失败：用户 {} 无 photo_wall:upload 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 photo_wall:upload 权限"})", "application/json");
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
                if (std::find(perms.begin(), perms.end(), "photo_wall:delete") == perms.end())
                {
                    spdlog::info("删除图片失败：用户 {} 无 photo_wall:delete 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 photo_wall:delete 权限"})", "application/json");
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

                // 权限检查：仅 blog:create 权限用户可新建博客
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "blog:create") == perms.end())
                {
                    spdlog::info("保存博客失败：用户 {} 无 blog:create 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 blog:create 权限"})", "application/json");
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

                const auto date = doc::valid_blog_date(body.value("update_time", ""));
                if (date.empty())
                {
                    spdlog::info("保存博客失败：缺少或无效的更新时间。");
                    res.status = 400;
                    res.set_content(R"({"error":"缺少或无效的更新时间"})", "application/json");
                    return;
                }

                const auto err = doc::save_blog(
                    conn, title, description, category, tagList,
                    pathCat, pathName,
                    content, date
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

                // 权限检查：博客编辑页需 blog:edit；后台管理页（from_manage）仅需 manage:edit
                const bool  from_manage = body.value("from_manage", false);
                const auto& perms       = session->permissions;
                const bool  allowed     = from_manage
                                        ? std::find(perms.begin(), perms.end(), "manage:edit") != perms.end()
                                        : std::find(perms.begin(), perms.end(), "blog:edit") != perms.end();
                if (!allowed)
                {
                    spdlog::info(
                        "更新博客失败：用户 {} 缺少{}权限。",
                        session->user_id,
                        from_manage ? " manage:edit" : " blog:edit"
                    );
                    res.status = 403;
                    res.set_content(
                        nlohmann::json{{"error", from_manage ? "当前用户无 manage:edit 权限"
                                                             : "当前用户无 blog:edit 权限"}}.dump(),
                        "application/json"
                    );
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

                const auto date = doc::valid_blog_date(body.value("update_time", ""));
                if (date.empty())
                {
                    spdlog::info("更新博客失败：缺少或无效的更新时间。");
                    res.status = 400;
                    res.set_content(R"({"error":"缺少或无效的更新时间"})", "application/json");
                    return;
                }

                const auto err = doc::update_blog(
                    conn, title, description, category, tagList,
                    old_file_path, pathCat, pathName, content, date
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

                // 权限检查：仅 blog:delete 权限用户可删除博客
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "blog:delete") == perms.end())
                {
                    spdlog::info("删除博客失败：用户 {} 无 blog:delete 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 blog:delete 权限"})", "application/json");
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

    void handle_download_blog(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "text/markdown");

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
                    spdlog::info("博客下载失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 权限检查：仅 blog:download 权限用户可下载
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "blog:download") == perms.end())
                {
                    spdlog::info("博客下载失败：用户 {} 无 blog:download 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 blog:download 权限"})", "application/json");
                    return;
                }

                if (!req.has_param("file_path"))
                {
                    spdlog::error("博客下载失败：缺少 file_path 参数。");
                    res.status = 400;
                    res.set_content(R"({"error":"缺少 file_path 参数"})", "application/json");
                    return;
                }
                const auto fp = req.get_param_value("file_path");
                auto       blog = doc::get_blog_by_file_path(conn, fp);
                if (!blog)
                {
                    spdlog::error("博客下载失败：{} 不存在。", fp);
                    res.status = 404;
                    res.set_content(R"({"error":"博客不存在"})", "application/json");
                    return;
                }
                const auto& safe_fp = blog->file_path.value_or(fp);

                const auto blogs_root = std::filesystem::path{ config::env()["FILE_PATH"] } / "doc" / "blogs";
                const auto md_path    = blogs_root / (safe_fp + ".md");

                // 防目录穿越：解析后的文件必须仍在博客目录内
                std::error_code ec;
                const auto resolved_root = std::filesystem::weakly_canonical(blogs_root, ec);
                if (ec)
                {
                    spdlog::error("博客下载失败：博客目录不可用（{}）。", ec.message());
                    res.status = 500;
                    res.set_content(R"({"error":"博客目录不可用"})", "application/json");
                    return;
                }
                const auto resolved_md = std::filesystem::weakly_canonical(md_path, ec);
                const auto rel         = std::filesystem::relative(resolved_md, resolved_root, ec);
                if (ec || rel.empty() || rel.string().starts_with(".."))
                {
                    spdlog::error("博客下载失败：非法文件路径 {}", safe_fp);
                    res.status = 400;
                    res.set_content(R"({"error":"非法文件路径"})", "application/json");
                    return;
                }

                std::ifstream ifs{ resolved_md, std::ios::binary };
                if (!ifs)
                {
                    spdlog::error("博客下载失败：读取文件 {} 失败。", resolved_md.string());
                    res.status = 404;
                    res.set_content(R"({"error":"博客文件不存在"})", "application/json");
                    return;
                }
                std::ostringstream oss;
                oss << ifs.rdbuf();

                // 附件文件名：博客文件相对路径的末级文件名（含 .md）
                const auto file_name = std::filesystem::path{ safe_fp + ".md" }.filename().string();
                res.set_header(
                    "Content-Disposition",
                    "attachment; filename=\"blog.md\"; filename*=UTF-8''" + percent_encode(file_name)
                );
                res.set_content(oss.str(), "text/markdown");
                spdlog::info("博客下载成功：{}。", safe_fp);
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
                if (std::find(perms.begin(), perms.end(), "introduction:edit") == perms.end())
                {
                    spdlog::info("更新个人简介失败：用户 {} 无 introduction:edit 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 introduction:edit 权限"})", "application/json");
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
