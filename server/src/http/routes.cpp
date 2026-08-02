/**
 * @file http/routes.cpp
 * @brief HTTP 路由实现
 */

#include "http/routes.h"
#include "auth/rate_limit.h"
#include "about/about_queries.h"
#include "auth/login.h"
#include "auth/session.h"
#include "blog/blog_queries.h"
#include "img/image_queries.h"
#include "md/markdown_parser.h"
#include "profile/profile_queries.h"
#include "config/env.h"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <format>
#include <mutex>
#include <sstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace http
{

static std::string FRONTEND_ORIGIN;
static std::string SERVER_HOST;
static int         SERVER_PORT;

/// libpqxx connection 非线程安全，所有数据库操作串行化
static std::mutex g_db_mutex;

void init()
{
    FRONTEND_ORIGIN =           config::get_env("FRONTEND_ORIGIN");
    SERVER_HOST     =           config::get_env("SERVER_HOST");
    SERVER_PORT     = std::stoi(config::get_env("SERVER_PORT"));

    if (SERVER_PORT <= 0)
    {
        spdlog::error("环境变量 SERVER_PORT 必须是有效的端口号！");
        std::exit(1);
    }
}

// ── 返回环境变量 ──

auto frontend_origin() -> std::string
{
    return FRONTEND_ORIGIN;
}

auto server_host() -> std::string
{
    return SERVER_HOST;
}

auto server_port() -> int
{
    return SERVER_PORT;
}

// ── 路由处理函数 ──

/**
 * @brief CORS 预检响应。
 */
static void handle_cors(httplib::Response& res, const std::string& allowed)
{
    res.set_header("Access-Control-Allow-Origin",  allowed);
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.status = 204;
}

/**
 * @brief 处理 POST /api/login/key 请求。
 */
static void handle_login_key(
    const httplib::Request&  req,
    httplib::Response&       res,
    pqxx::connection&        conn,
    const std::string&       allowed)
{
    std::lock_guard<std::mutex> lock{ g_db_mutex };
    res.set_header("Access-Control-Allow-Origin", allowed);
    res.set_header("Content-Type", "application/json");

    // 解析 JSON
    spdlog::debug("收到密钥登录请求（ip={}）。", req.remote_addr);

    const auto body = nlohmann::json::parse(req.body, nullptr, false);
    if (body.is_discarded())
    {
        spdlog::error("密钥登录失败：无效的 JSON。");
        res.status = 400;
        res.set_content(R"({"error":"无效的 JSON"})", "application/json");
        return;
    }

    // 登录频率限制检查
    const auto& ip = req.remote_addr;
    if (auth::is_rate_limited(ip))
    {
        spdlog::error("密钥登录失败：IP {} 已被限流。", ip);
        res.status = 429;
        res.set_content(R"({"error":"登录尝试过于频繁，请稍后再试"})", "application/json");
        return;
    }

    // 调用登录逻辑
    const auto key = body.value("key", "");
    auto result = auth::login_by_key(conn, key);

    if (!result)
    {
        spdlog::error("密钥登录失败（ip={}）：{}", ip, result.error());
        res.status = 401;
        auth::record_failure(ip);
        nlohmann::json err;
        err["error"] = result.error();
        res.set_content(err.dump(), "application/json");
        return;
    }
    auth::clear(ip);
    spdlog::info("密钥登录成功（ip={}，user_id={}）。", ip, result->id);
    nlohmann::json resp;
    resp["id"] = result->id;
    resp["username"] = result->username.has_value()
        ? nlohmann::json(*result->username)
        : nlohmann::json(nullptr);
    resp["token"] = auth::create_session(conn, result->id, result->permissions);
    res.set_content(resp.dump(), "application/json");
}

/**
 * @brief 处理 POST /api/login/password 请求。
 */
static void handle_login_password(
    const httplib::Request&  req,
    httplib::Response&       res,
    pqxx::connection&        conn,
    const std::string&       allowed)
{
    std::lock_guard<std::mutex> lock{ g_db_mutex };
    res.set_header("Access-Control-Allow-Origin", allowed);
    res.set_header("Content-Type", "application/json");

    // 解析 JSON
    spdlog::debug("收到密码登录请求（ip={}）。", req.remote_addr);

    const auto body = nlohmann::json::parse(req.body, nullptr, false);
    if (body.is_discarded())
    {
        spdlog::error("密码登录失败：无效的 JSON。");
        res.status = 400;
        res.set_content(R"({"error":"无效的 JSON"})", "application/json");
        return;
    }

    // 登录频率限制检查
    const auto& ip = req.remote_addr;
    if (auth::is_rate_limited(ip))
    {
        spdlog::error("密码登录失败：IP {} 已被限流。", ip);
        res.status = 429;
        res.set_content(R"({"error":"登录尝试过于频繁，请稍后再试"})", "application/json");
        return;
    }

    // 调用登录逻辑
    const auto username = body.value("username", "");
    const auto pwd      = body.value("password", "");
    auto result = auth::login_by_password(conn, username, pwd);

    if (!result)
    {
        spdlog::error("密码登录失败（ip={}，user={}）：{}", ip, username, result.error());
        res.status = 401;
        auth::record_failure(ip);
        nlohmann::json err;
        err["error"] = result.error();
        res.set_content(err.dump(), "application/json");
        return;
    }
    auth::clear(ip);
    spdlog::info("密码登录成功（ip={}，user={}）。", ip, username);
    nlohmann::json resp;
    resp["id"] = result->id;
    resp["username"] = result->username.has_value()
        ? nlohmann::json(*result->username)
        : nlohmann::json(nullptr);
    resp["token"] = auth::create_session(conn, result->id, result->permissions);
    res.set_content(resp.dump(), "application/json");
}

/**
 * @brief 处理 GET /api/categories 请求。
 */
static void handle_get_categories(
    httplib::Response& res,
    pqxx::connection&  conn,
    const std::string& allowed)
{
    std::lock_guard<std::mutex> lock{ g_db_mutex };
    res.set_header("Access-Control-Allow-Origin", allowed);
    res.set_header("Content-Type", "application/json");

    auto categories = blog::get_categories(conn);
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

/**
 * @brief 处理 GET /api/tags 请求。
 */
static void handle_get_tags(
    const httplib::Request& req,
    httplib::Response&      res,
    pqxx::connection&       conn,
    const std::string&      allowed)
{
    std::lock_guard<std::mutex> lock{ g_db_mutex };
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

    auto tags = blog::get_tags(conn, category_ids);
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

/**
 * @brief 处理 GET /api/blogs 请求。
 */
static void handle_get_blogs(
    const httplib::Request&  req,
    httplib::Response&       res,
    pqxx::connection&        conn,
    const std::string&       allowed)
{
    std::lock_guard<std::mutex> lock{ g_db_mutex };
    res.set_header("Access-Control-Allow-Origin", allowed);
    res.set_header("Content-Type", "application/json");

    blog::BlogQuery query;

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

    auto blogs = blog::get_blogs(conn, query);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& b : blogs)
    {
        nlohmann::json item;
        item["id"]         = b.id;
        item["title"]      = b.title;
        item["description"] = b.description.has_value()
            ? nlohmann::json(*b.description) : nlohmann::json(nullptr);
        item["update_time"] = b.update_time;
        item["category"]   = b.category.has_value()
            ? nlohmann::json(*b.category) : nlohmann::json(nullptr);
        item["tags"]       = b.tags;
        item["file_path"]   = b.file_path.has_value() ? nlohmann::json(*b.file_path) : nlohmann::json(nullptr);
        arr.push_back(std::move(item));
    }
    res.set_content(arr.dump(), "application/json");
}

/**
 * @brief 处理 POST /api/blog/save 请求。
 */
static void handle_blog_save(
    const httplib::Request& req,
    httplib::Response&      res,
    pqxx::connection&        conn,
    const std::string&       allowed)
{
    std::lock_guard<std::mutex> lock{ g_db_mutex };
    res.set_header("Access-Control-Allow-Origin", allowed);
    res.set_header("Content-Type", "application/json");

    const auto body = nlohmann::json::parse(req.body, nullptr, false);
    if (body.is_discarded()) {
        spdlog::error("保存博客失败：无效的 JSON。");
        res.status = 400;
        res.set_content(R"({"error":"无效的 JSON"})", "application/json");
        return;
    }

    // Session 验证
    std::string token;
    if (req.has_header("Authorization")) {
        const auto& auth_hdr = req.get_header_value("Authorization");
        constexpr std::string_view PREFIX = "Bearer ";
        if (auth_hdr.size() > PREFIX.size() &&
            auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
            token = auth_hdr.substr(PREFIX.size());
    }
    const auto session = auth::validate_session(conn, token);
    if (!session) {
        spdlog::error("保存博客失败：未登录或会话已过期。");
        res.status = 401;
        res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
        return;
    }

    // 权限检查
    const auto& perms = session->permissions;
    if (std::find(perms.begin(), perms.end(), "create") == perms.end()) {
        spdlog::error("保存博客失败：用户 {} 无 create 权限。", session->user_id);
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

    if (title.empty() || description.empty() || category.empty() ||
        pathCat.empty() || pathName.empty() || content.empty()) {
        spdlog::error("保存博客失败：缺少必填字段。");
        res.status = 400;
        res.set_content(R"({"error":"所有字段均为必填"})", "application/json");
        return;
    }

    std::vector<std::string> tagList;
    if (tagsJson.is_array())
        for (const auto& t : tagsJson)
            if (t.is_string()) tagList.push_back(t.get<std::string>());

    std::time_t now = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", std::localtime(&now));

    const auto err = blog::save_blog(
        conn, title, description, category, tagList,
        pathCat, pathName,
        content, buf);

    if (err) {
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

/**
 * @brief 处理 PUT /api/blog/update 请求。
 */
static void handle_blog_update(
    const httplib::Request& req,
    httplib::Response&      res,
    pqxx::connection&        conn,
    const std::string&       allowed)
{
    std::lock_guard<std::mutex> lock{ g_db_mutex };
    res.set_header("Access-Control-Allow-Origin", allowed);
    res.set_header("Content-Type", "application/json");

    const auto body = nlohmann::json::parse(req.body, nullptr, false);
    if (body.is_discarded()) {
        spdlog::error("更新博客失败：无效的 JSON。");
        res.status = 400;
        res.set_content(R"({"error":"无效的 JSON"})", "application/json");
        return;
    }

    // Session 验证
    std::string token;
    if (req.has_header("Authorization")) {
        const auto& auth_hdr = req.get_header_value("Authorization");
        constexpr std::string_view PREFIX = "Bearer ";
        if (auth_hdr.size() > PREFIX.size() &&
            auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
            token = auth_hdr.substr(PREFIX.size());
    }
    const auto session = auth::validate_session(conn, token);
    if (!session) {
        spdlog::error("更新博客失败：未登录或会话已过期。");
        res.status = 401;
        res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
        return;
    }

    // 权限检查
    const auto& perms = session->permissions;
    if (std::find(perms.begin(), perms.end(), "edit") == perms.end()) {
        spdlog::error("更新博客失败：用户 {} 无 edit 权限。", session->user_id);
        res.status = 403;
        res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
        return;
    }

    const auto title       = body.value("title", "");
    const auto description = body.value("description", "");
    const auto category    = body.value("category", "");
    const auto content     = body.value("content", "");
    const auto pathCat     = body.value("file_path_category", "");
    const auto pathName    = body.value("file_path_name", "");
    const auto old_file_path = body.value("old_file_path", "");
    const auto tagsJson    = body.value("tags", nlohmann::json::array());

    if (title.empty() || description.empty() || category.empty() ||
        pathCat.empty() || pathName.empty() || old_file_path.empty() || content.empty()) {
        spdlog::error("更新博客失败：缺少必填字段。");
        res.status = 400;
        res.set_content(R"({"error":"所有字段均为必填"})", "application/json");
        return;
    }

    std::vector<std::string> tagList;
    if (tagsJson.is_array())
        for (const auto& t : tagsJson)
            if (t.is_string()) tagList.push_back(t.get<std::string>());

    std::time_t now = std::time(nullptr);
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", std::localtime(&now));

    const auto err = blog::update_blog(
        conn, title, description, category, tagList,
        old_file_path, pathCat, pathName, content, buf);

    if (err) {
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

// ── 路由注册 ──

void setup_routes(httplib::Server& svr, pqxx::connection& conn)
{
    const std::string allowed = frontend_origin();

    svr.Options("/api/.*",
        [allowed](const auto&, auto& res)
        {
            handle_cors(res, allowed);
        }
    );

    svr.Post("/api/login/key",
        [&conn, allowed](const auto& req, auto& res)
        {
            handle_login_key(req, res, conn, allowed);
        }
    );

    svr.Post("/api/login/password",
        [&conn, allowed](const auto& req, auto& res)
        {
            handle_login_password(req, res, conn, allowed);
        }
    );

    svr.Get("/api/user/permissions",
        [&conn, allowed](const auto& req, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");

            // Session 验证：仅返回已登录用户自身的权限
            std::string token;
            if (req.has_header("Authorization")) {
                const auto& auth_hdr = req.get_header_value("Authorization");
                constexpr std::string_view PREFIX = "Bearer ";
                if (auth_hdr.size() > PREFIX.size() &&
                    auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                    token = auth_hdr.substr(PREFIX.size());
            }
            const auto session = auth::validate_session(conn, token);
            if (!session) {
                spdlog::error("获取权限失败：未登录或会话已过期。");
                res.status = 401;
                res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                return;
            }

            res.set_content(nlohmann::json{{"permissions", session->permissions}}.dump(), "application/json");
        }
    );

    svr.Get("/api/categories",
        [&conn, allowed](const auto&, auto& res)
        {
            handle_get_categories(res, conn, allowed);
        }
    );

    svr.Get("/api/tags",
        [&conn, allowed](const auto& req, auto& res)
        {
            handle_get_tags(req, res, conn, allowed);
        }
    );

    svr.Get("/api/blogs",
        [&conn, allowed](const auto& req, auto& res)
        {
            handle_get_blogs(req, res, conn, allowed);
        }
    );

    svr.Get("/api/blog",
        [&conn, allowed](const auto& req, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
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
            spdlog::info("正在获取博客：{}", fp);
            auto blog = blog::get_blog_by_file_path(conn, fp);
            if (!blog)
            {
                spdlog::info("获取博客失败：{} 不存在。", fp);
                res.status = 404;
                res.set_content(R"({"error":"博客不存在"})", "application/json");
                return;
            }

            nlohmann::json item;
            item["id"]          = blog->id;
            item["title"]       = blog->title;
            item["description"] = blog->description.has_value()
                ? nlohmann::json(*blog->description) : nlohmann::json(nullptr);
            item["content"]     = blog->content.has_value()
                ? nlohmann::json(*blog->content) : nlohmann::json(nullptr);
            item["update_time"]  = blog->update_time;
            item["category"]    = blog->category.has_value()
                ? nlohmann::json(*blog->category) : nlohmann::json(nullptr);
            item["file_path"]   = blog->file_path.has_value()
                ? nlohmann::json(*blog->file_path) : nlohmann::json(nullptr);
            item["tags"]        = blog->tags;
            res.set_content(item.dump(), "application/json");
        }
    );

    // POST /api/blog/parse — 解析 Markdown frontmatter (委托 md::parse_frontmatter)
    svr.Post("/api/blog/parse",
        [allowed](const auto& req, auto& res)
        {
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            auto result = md::parse_frontmatter(req.body);
            res.set_content(result.dump(), "application/json");
        }
    );

    // 挂载图片静态文件服务
    svr.set_mount_point("/image", img::image_path());

    // GET /api/images — 获取所有图片
    svr.Get("/api/images",
        [&conn, allowed](const auto&, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(img::get_all_images(conn).dump(), "application/json");
        }
    );

    // POST /api/image/save — 保存/更新图片元数据（需要 edit 权限）
    svr.Post("/api/image/save",
        [&conn, allowed](const auto& req, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");

            // Session 验证
            std::string token;
            if (req.has_header("Authorization")) {
                const auto& auth_hdr = req.get_header_value("Authorization");
                constexpr std::string_view PREFIX = "Bearer ";
                if (auth_hdr.size() > PREFIX.size() &&
                    auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                    token = auth_hdr.substr(PREFIX.size());
            }
            const auto session = auth::validate_session(conn, token);
            if (!session) {
                spdlog::error("保存图片元数据失败：未登录或会话已过期。");
                res.status = 401;
                res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                return;
            }
            const auto& perms = session->permissions;
            if (std::find(perms.begin(), perms.end(), "edit") == perms.end()) {
                spdlog::error("保存图片元数据失败：用户 {} 无 edit 权限。", session->user_id);
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
            if (!err.empty()) {
                spdlog::error("保存图片元数据失败：{}", err);
                res.status = 500;
                nlohmann::json j;
                j["error"] = err;
                res.set_content(j.dump(), "application/json");
                return;
            }
            spdlog::info("图片元数据保存成功：{}", body.value("path", ""));
            res.set_content(R"({"ok":true})", "application/json");
        }
    );

    // POST /api/image/upload — 上传图片文件（需要 edit 权限）
    svr.Post("/api/image/upload",
        [&conn, allowed](const auto& req, auto& res)
        {
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");

            if (!req.form.has_file("file")) {
                spdlog::error("上传图片失败：未选择文件。");
                res.status = 400;
                res.set_content(R"({"error":"未选择文件"})", "application/json");
                return;
            }
            const auto file = req.form.get_file("file");

            std::lock_guard<std::mutex> lock{ g_db_mutex };

            // Session 验证
            std::string token;
            if (req.has_header("Authorization")) {
                const auto& auth_hdr = req.get_header_value("Authorization");
                constexpr std::string_view PREFIX = "Bearer ";
                if (auth_hdr.size() > PREFIX.size() &&
                    auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                    token = auth_hdr.substr(PREFIX.size());
            }
            const auto session = auth::validate_session(conn, token);
            if (!session) {
                spdlog::error("上传图片失败：未登录或会话已过期。");
                res.status = 401;
                res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                return;
            }
            const auto& perms = session->permissions;
            if (std::find(perms.begin(), perms.end(), "edit") == perms.end()) {
                spdlog::error("上传图片失败：用户 {} 无 edit 权限。", session->user_id);
                res.status = 403;
                res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                return;
            }

            auto [err, result] = img::upload_image(conn, file.filename, file.content);
            if (!err.empty()) {
                spdlog::error("上传图片失败：{}", err);
                res.status = 500;
                nlohmann::json j;
                j["error"] = err;
                res.set_content(j.dump(), "application/json");
                return;
            }
            spdlog::info("图片上传成功：{}。", file.filename);
            res.set_content(result.dump(), "application/json");
        }
    );

    // DELETE /api/image/delete — 删除图片（需要 edit 权限）
    svr.Delete("/api/image/delete",
        [&conn, allowed](const auto& req, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");

            // Session 验证
            std::string token;
            if (req.has_header("Authorization")) {
                const auto& auth_hdr = req.get_header_value("Authorization");
                constexpr std::string_view PREFIX = "Bearer ";
                if (auth_hdr.size() > PREFIX.size() &&
                    auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                    token = auth_hdr.substr(PREFIX.size());
            }
            const auto session = auth::validate_session(conn, token);
            if (!session) {
                spdlog::error("删除图片失败：未登录或会话已过期。");
                res.status = 401;
                res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                return;
            }
            const auto& perms = session->permissions;
            if (std::find(perms.begin(), perms.end(), "edit") == perms.end()) {
                spdlog::error("删除图片失败：用户 {} 无 edit 权限。", session->user_id);
                res.status = 403;
                res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                return;
            }

            const auto body = nlohmann::json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                spdlog::error("删除图片失败：无效的 JSON。");
                res.status = 400;
                res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                return;
            }

            const auto path = body.value("path", "");
            const auto err = img::delete_image(conn, path);
            if (!err.empty()) {
                spdlog::error("删除图片失败：{}", err);
                res.status = 500;
                nlohmann::json j;
                j["error"] = err;
                res.set_content(j.dump(), "application/json");
                return;
            }
            spdlog::info("图片删除成功：{}。", path);
            res.set_content(R"({"ok":true})", "application/json");
            res.set_content(R"({"ok":true})", "application/json");
        }
    );

    // POST /api/blog/save — 保存博客
    svr.Post("/api/blog/save",
        [&conn, allowed](const auto& req, auto& res)
        {
            handle_blog_save(req, res, conn, allowed);
        }
    );

    // PUT /api/blog/update — 编辑已有博客
    svr.Put("/api/blog/update",
        [&conn, allowed](const auto& req, auto& res)
        {
            handle_blog_update(req, res, conn, allowed);
        }
    );

    // DELETE /api/blog/delete — 删除博客
    svr.Delete("/api/blog/delete",
        [&conn, allowed](const auto& req, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");

            // Session 验证
            std::string token;
            if (req.has_header("Authorization")) {
                const auto& auth_hdr = req.get_header_value("Authorization");
                constexpr std::string_view PREFIX = "Bearer ";
                if (auth_hdr.size() > PREFIX.size() &&
                    auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                    token = auth_hdr.substr(PREFIX.size());
            }
            const auto session = auth::validate_session(conn, token);
            if (!session) {
                spdlog::error("删除博客失败：未登录或会话已过期。");
                res.status = 401;
                res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                return;
            }

            // 权限检查
            const auto& perms = session->permissions;
            if (std::find(perms.begin(), perms.end(), "drop") == perms.end()) {
                spdlog::error("删除博客失败：用户 {} 无 drop 权限。", session->user_id);
                res.status = 403;
                res.set_content(R"({"error":"当前用户无 drop 权限"})", "application/json");
                return;
            }

            const auto body = nlohmann::json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
                spdlog::error("删除博客失败：无效的 JSON。");
                res.status = 400;
                res.set_content(R"({"error":"无效的 JSON"})", "application/json");
                return;
            }

            const auto file_path = body.value("file_path", "");
            if (file_path.empty()) {
                spdlog::error("删除博客失败：缺少 file_path 参数。");
                res.status = 400;
                res.set_content(R"({"error":"缺少 file_path 参数"})", "application/json");
                return;
            }

            const auto err = blog::delete_blog(conn, file_path);
            if (err) {
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

    // GET /api/about — 获取《关于我》README 内容（从数据库读取）
    svr.Get("/api/about",
        [&conn, allowed](const auto&, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(nlohmann::json{{"content", about::get_about(conn)}}.dump(), "application/json");
        }
    );

    // GET /api/profile — 获取个人介绍
    svr.Get("/api/profile",
        [&conn, allowed](const auto&, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(profile::get_profile(conn).dump(), "application/json");
        }
    );

    // POST /api/profile/save — 更新个人介绍（需要 edit 权限）
    svr.Post("/api/profile/save",
        [&conn, allowed](const auto& req, auto& res)
        {
            std::lock_guard<std::mutex> lock{ g_db_mutex };
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");

            // Session 验证
            std::string token;
            if (req.has_header("Authorization")) {
                const auto& auth_hdr = req.get_header_value("Authorization");
                constexpr std::string_view PREFIX = "Bearer ";
                if (auth_hdr.size() > PREFIX.size() &&
                    auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                    token = auth_hdr.substr(PREFIX.size());
            }
            const auto session = auth::validate_session(conn, token);
            if (!session) {
                spdlog::error("更新个人简介失败：未登录或会话已过期。");
                res.status = 401;
                res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                return;
            }
            const auto& perms = session->permissions;
            if (std::find(perms.begin(), perms.end(), "edit") == perms.end()) {
                spdlog::error("更新个人简介失败：用户 {} 无 edit 权限。", session->user_id);
                res.status = 403;
                res.set_content(R"({"error":"当前用户无 edit 权限"})", "application/json");
                return;
            }

            const auto body = nlohmann::json::parse(req.body, nullptr, false);
            if (body.is_discarded()) {
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
            if (!err.empty()) {
                spdlog::error("更新个人简介失败：{}", err);
                res.status = 500;
                nlohmann::json j;
                j["error"] = err;
                res.set_content(j.dump(), "application/json");
                return;
            }
            spdlog::info("个人简介更新成功。");
            res.set_content(R"({"ok":true})", "application/json");
        }
    );
}

} // namespace http
