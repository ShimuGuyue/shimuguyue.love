/**
 * @file http/handlers/blog_edit.cpp
 * @brief 博客新建 / 编辑 / 删除 / 下载 HTTP 路由处理函数实现
 */

#include "http/handlers/blog_edit.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "auth/session.h"
#include "cache/cache.h"
#include "config/config.h"
#include "db/connection_pool.h"
#include "doc/blog_queries.h"

namespace
{
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

                spdlog::info("收到博客保存请求");
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("保存博客失败：无效的 JSON。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "无效的 JSON";
                    res.set_content(err.dump(), "application/json");
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

                    nlohmann::json err;
                    err["error"] = "未登录或会话已过期";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 权限检查：blog:create
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "blog:create") == perms.end())
                {
                    spdlog::info("保存博客失败：用户 {} 无 blog:create 权限。", session->user_id);
                    res.status = 403;

                    nlohmann::json err;
                    err["error"] = "当前用户无 blog:create 权限";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                const auto title           = body.value("title", "");
                const auto description     = body.value("description", "");
                const auto categories      = body.value("categories", nlohmann::json::array());
                const auto tags            = body.value("tags", nlohmann::json::array());
                const auto path_categories = body.value("file_path_category", "");
                const auto path_name       = body.value("file_path_name", "");
                const auto content         = body.value("content", "");

                // 字段校验
                for (auto field : nlohmann::json{ title, description, categories, tags, path_categories, path_name, content })
                {
                    if (field.empty())
                    {
                        spdlog::info("保存博客失败：缺少必填字段。");
                        res.status = 400;

                        nlohmann::json err;
                        err["error"] = "所有字段均为必填";
                        res.set_content(err.dump(), "application/json");
                        return;
                    }
                }

                std::vector<std::string> category_list;
                if (categories.is_array())
                {
                    for (const auto &c : categories)
                    {
                        if (c.is_string())
                            category_list.push_back(c.template get<std::string>());
                    }
                }
                if (category_list.empty())
                {
                    spdlog::info("保存博客失败：缺少分类。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "所有字段均为必填";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                std::vector<std::string> tag_list;
                if (tags.is_array())
                {
                    for (const auto &t : tags)
                    {
                        if (t.is_string())
                            tag_list.push_back(t.template get<std::string>());
                    }
                }

                const auto date = doc::valid_blog_date(body.value("update_time", ""));
                if (date.empty())
                {
                    spdlog::info("保存博客失败：缺少或无效的更新时间。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "缺少或无效的更新时间";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 调用博客保存逻辑
                const auto err = doc::save_blog(
                    conn, title, description, category_list, tag_list,
                    path_categories, path_name,
                    content, date
                );
                if (err)
                {
                    spdlog::error("保存博客失败：{}", *err);
                    res.status = 500;

                    nlohmann::json jerr;
                    jerr["error"] = *err;
                    res.set_content(jerr.dump(), "application/json");
                    return;
                }

                spdlog::info("博客保存成功：{}/{}。", path_categories, path_name);

                nlohmann::json success;
                success["ok"] = true;
                res.set_content(success.dump(), "application/json");

                // 删除旧缓存
                cache::invalidate_prefix("api-cache:/api/blogs");
                cache::invalidate_prefix("api-cache:/api/categories");
                cache::invalidate_prefix("api-cache:/api/tags");
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

                spdlog::info("收到博客修改请求。");
                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("更新博客失败：无效的 JSON。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "无效的 JSON";
                    res.set_content(err.dump(), "application/json");
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

                    nlohmann::json err;
                    err["error"] = "未登录或会话已过期";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 权限检查：博客编辑页需 blog:edit；后台管理页（from_manage）需 manage:edit
                const bool from_manage = body.value("from_manage", false);
                const auto& perms = session->permissions;
                const bool allowed = from_manage
                                   ? std::find(perms.begin(), perms.end(), "manage:edit") != perms.end()
                                   : std::find(perms.begin(), perms.end(), "blog:edit") != perms.end();
                if (!allowed)
                {
                    spdlog::info("更新博客失败：用户 {} 缺少 {} 权限。",
                        session->user_id,
                        from_manage ? " manage:edit" : " blog:edit"
                    );
                    res.status = 403;

                    nlohmann::json err;
                    err["error"] = std::format("当前用户无 {} 权限", from_manage ? "manage:edit" : "blog:edit");
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                const auto title           = body.value("title", "");
                const auto description     = body.value("description", "");
                const auto content         = body.value("content", "");
                const auto path_categories = body.value("file_path_category", "");
                const auto path_name       = body.value("file_path_name", "");
                const auto old_file_path   = body.value("old_file_path", "");
                const auto categories      = body.value("categories", nlohmann::json::array());
                const auto tags            = body.value("tags", nlohmann::json::array());

                // 字段校验
                for (auto field : nlohmann::json{ title, description, categories, tags, path_categories, path_name, content })
                {
                    if (field.empty())
                    {
                        spdlog::info("保存博客失败：缺少必填字段。");
                        res.status = 400;

                        nlohmann::json err;
                        err["error"] = "所有字段均为必填";
                        res.set_content(err.dump(), "application/json");
                        return;
                    }
                }

                std::vector<std::string> category_list;
                if (categories.is_array())
                {
                    for (const auto &c : categories)
                    {
                        if (c.is_string())
                            category_list.push_back(c.template get<std::string>());
                    }
                }
                if (category_list.empty())
                {
                    spdlog::info("更新博客失败：缺少分类。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "所有字段均为必填";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                std::vector<std::string> tag_list;
                if (tags.is_array())
                {
                    for (const auto &t : tags)
                    {
                        if (t.is_string())
                            tag_list.push_back(t.template get<std::string>());
                    }
                }

                const auto date = doc::valid_blog_date(body.value("update_time", ""));
                if (date.empty())
                {
                    spdlog::info("更新博客失败：缺少或无效的更新时间。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "缺少或无效的更新时间";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 调用更新博客逻辑
                const auto err = doc::update_blog(
                    conn, title, description, category_list, tag_list,
                    old_file_path, path_categories, path_name, content, date
                );
                if (err)
                {
                    spdlog::error("更新博客失败：{}", *err);
                    res.status = 500;

                    nlohmann::json jerr;
                    jerr["error"] = *err;
                    res.set_content(jerr.dump(), "application/json");
                    return;
                }

                spdlog::info("博客更新成功：{}/{}。", path_categories, path_name);

                nlohmann::json success;
                success["ok"] = true;
                res.set_content(success.dump(), "application/json");

                // 删除旧缓存
                cache::invalidate_prefix("api-cache:/api/blogs");
                cache::invalidate_prefix("api-cache:/api/categories");
                cache::invalidate_prefix("api-cache:/api/tags");
                cache::del(cache::cache_key("/api/blog", { { "file_path", old_file_path } }));
                cache::del(cache::cache_key("/api/blog", { { "file_path", path_categories + "/" + path_name } }));
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

                spdlog::info("收到博客删除请求。");
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

                    nlohmann::json err;
                    err["error"] = "未登录或会话已过期";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 权限检查：blog:delete
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "blog:delete") == perms.end())
                {
                    spdlog::info("删除博客失败：用户 {} 无 blog:delete 权限。", session->user_id);
                    res.status = 403;

                    nlohmann::json err;
                    err["error"] = "当前用户无 blog:delete 权限";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("删除博客失败：无效的 JSON。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "无效的 JSON";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                const auto file_path = body.value("file_path", "");
                if (file_path.empty())
                {
                    spdlog::error("删除博客失败：缺少 file_path 参数。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "缺少 file_path 参数";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 调用博客删除逻辑
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
                nlohmann::json success;
                success["ok"] = true;
                res.set_content(success.dump(), "application/json");

                // 删除旧缓存
                cache::invalidate_prefix("api-cache:/api/blogs");
                cache::invalidate_prefix("api-cache:/api/categories");
                cache::invalidate_prefix("api-cache:/api/tags");
                cache::del(cache::cache_key("/api/blog", { { "file_path", file_path } }));
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

                spdlog::debug("收到博客下载请求。");
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
                    spdlog::debug("博客下载失败：未登录或会话已过期。");
                    res.status = 401;

                    nlohmann::json err;
                    err["error"] = "未登录或会话已过期";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                // 权限检查：blog:download
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "blog:download") == perms.end())
                {
                    spdlog::debug("博客下载失败：用户 {} 无 blog:download 权限。", session->user_id);
                    res.status = 403;

                    nlohmann::json err;
                    err["error"] = "当前用户无 blog:download 权限";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                if (!req.has_param("file_path"))
                {
                    spdlog::debug("博客下载失败：缺少 file_path 参数。");
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "缺少 file_path 参数";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                const std::string& file_path = req.get_param_value("file_path");
                auto blog = doc::get_blog_by_file_path(conn, file_path);
                if (!blog)
                {
                    spdlog::debug("博客下载失败：{} 不存在。", file_path);
                    res.status = 404;

                    nlohmann::json err;
                    err["error"] = "博客不存在";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                const auto blogs_root = std::filesystem::path{ config::config()["FILE_PATH"] } / "blogs";
                const auto md_path    = blogs_root / (file_path + ".md");

                // 文件存在及权限检查
                std::error_code ec;
                const auto resolved_root = std::filesystem::weakly_canonical(blogs_root, ec);
                if (ec)
                {
                    spdlog::error("博客下载失败：博客目录不可用（{}）。", ec.message());
                    res.status = 500;

                    nlohmann::json err;
                    err["error"] = "博客目录不可用";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                const auto resolved_md = std::filesystem::weakly_canonical(md_path, ec);
                const auto rel         = std::filesystem::relative(resolved_md, resolved_root, ec);
                if (ec || rel.empty() || rel.string().starts_with("../"))
                {
                    spdlog::error("博客下载失败：非法文件路径 {}", file_path);
                    res.status = 400;

                    nlohmann::json err;
                    err["error"] = "非法文件路径";
                    res.set_content(err.dump(), "application/json");
                    return;
                }

                std::ifstream ifs{ resolved_md, std::ios::binary };
                if (!ifs)
                {
                    spdlog::error("博客下载失败：读取文件 {} 失败。", resolved_md.string());
                    res.status = 404;

                    nlohmann::json err;
                    err["error"] = "博客文件不存在";
                    res.set_content(err.dump(), "application/json");
                    return;
                }
                std::ostringstream oss;
                oss << ifs.rdbuf();

                // 附件文件名：博客文件相对路径的末级文件名（含 .md）
                const auto file_name = std::filesystem::path{ file_path + ".md" }.filename().string();
                res.set_header(
                    "Content-Disposition",
                    "attachment; filename=\"blog.md\"; filename*=UTF-8''" + percent_encode(file_name)
                );
                res.set_content(oss.str(), "text/markdown");
                spdlog::debug("博客下载成功：{}。", file_path);
            }
        );
    }

} // namespace http
