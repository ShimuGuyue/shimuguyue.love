/**
 * @file http/handlers/blog.cpp
 * @brief 博客公开查询 HTTP 路由处理函数实现
 */

#include "http/handlers/blog.h"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "cache/cache.h"
#include "config/config.h"
#include "db/connection_pool.h"
#include "doc/blog_queries.h"
#include "http/handlers/common.h"
#include "md/markdown_parser.h"

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
     * @brief 解析非负整数查询参数。
     * @param req      请求。
     * @param name     参数名。
     * @param fallback 参数缺失或非法（非数字、负数、含多余字符）时返回的默认值。
     * @return 解析得到的非负整数，或 fallback。
     */
    auto uint_param(const httplib::Request& req, const char* name, int fallback) -> int
    {
        if (!req.has_param(name))
            return fallback;

        const auto& raw = req.get_param_value(name);
        int value{ 0 };
        const auto [ptr, ec] = std::from_chars(
            raw.data(),
            raw.data() + raw.size(),
            value
        );
        if (ec != std::errc{} || ptr != raw.data() + raw.size() || value < 0)
            return fallback;

        return value;
    }

} // namespace





namespace http
{
    void handle_get_categories(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        spdlog::debug("收到博客分类列表获取请求。");

        nlohmann::json json;

        const auto key = cache::cache_key("/api/categories", {});
        if (const auto cached = cache::get(key); cached.has_value())
        {
            spdlog::debug("博客分类列表从缓存获取成功。");

            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(*cached, "application/json");
            return;
        }

        const auto err = db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                auto categories = doc::get_categories(conn);
                json = nlohmann::json::array();
                for (const auto& c : categories)
                {
                    nlohmann::json item;
                    item["id"]   = c.id;
                    item["name"] = c.name;
                    json.push_back(std::move(item));
                }

                spdlog::debug("博客分类列表从数据库获取成功。");

                res.set_content(json.dump(), "application/json");

                cache_set_list(key, res.body, std::stoll(config::config()["CACHE_TTL_CATEGORIES"]));

                spdlog::debug("博客分类列表已加入缓存。");
            }
        );
        if (err)
        {
            spdlog::warn("获取分类列表失败：{}", *err);
            res.status = 503;
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            json["error"] = "数据库繁忙，请稍后重试";
            res.set_content(json.dump(), "application/json");
        }
    }

    void handle_get_tags(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        spdlog::debug("收到博客标签列表获取请求。");

        nlohmann::json json;

        const auto key = cache::cache_key("/api/tags", {});
        if (const auto cached = cache::get(key); cached.has_value())
        {
            spdlog::debug("博客标签列表从缓存获取成功。");

            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(*cached, "application/json");
            return;
        }

        const auto err = db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                auto tags = doc::get_tags(conn);
                json = nlohmann::json::array();
                for (const auto& t : tags)
                {
                    nlohmann::json item;
                    item["id"]   = t.id;
                    item["name"] = t.name;
                    json.push_back(std::move(item));
                }

                spdlog::debug("博客标签列表从数据库获取成功。");

                res.set_content(json.dump(), "application/json");

                cache_set_list(key, res.body, std::stoll(config::config()["CACHE_TTL_TAGS"]));

                spdlog::debug("博客标签列表已加入缓存。");
            }
        );
        if (err)
        {
            spdlog::warn("获取标签列表失败：{}", *err);
            res.status = 503;
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            json["error"] = "数据库繁忙，请稍后重试";
            res.set_content(json.dump(), "application/json");
        }
    }

    void handle_get_blogs(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        spdlog::debug("收到博客列表获取请求。");

        nlohmann::json json;

        // 分页参数：page 从 1 开始
        const int page = std::max(1, uint_param(req, "page", 1));
        // 分页由 page / page_size 触发：显式传 0 表示不分页，一次取全量
        // page_size 缺省时取 conf/page_size.yml 的配置值
        int page_size{ 0 };
        if (req.has_param("page_size"))
            page_size = uint_param(req, "page_size", 0);
        else if (req.has_param("page"))
            page_size = std::stoi(config::config()["BLOGS_PAGESIZE"]);

        std::unordered_map<std::string, std::string> params;
        if (req.has_param("category_ids"))
            params["category_ids"] = normalize_id_list(req.get_param_value("category_ids"));
        if (req.has_param("tag_ids"))
            params["tag_ids"]      = normalize_id_list(req.get_param_value("tag_ids"));
        if (req.has_param("q"))
            params["q"]            = req.get_param_value("q");

        // 每页内容独立缓存，缓存键需带上分页参数
        if (page_size > 0)
        {
            params["page"]      = std::to_string(page);
            params["page_size"] = std::to_string(page_size);
        }
        const auto key = cache::cache_key("/api/blogs", params);

        if (const auto cached = cache::get(key); cached.has_value())
        {
            spdlog::debug("博客列表从缓存获取成功。");

            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(*cached, "application/json");
            return;
        }

        const auto err = db::with_db(
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

                query.page      = page;
                query.page_size = page_size;

                // 分页时先统计总数，供前端计算页数
                int total{ page_size > 0 ? doc::count_blogs(conn, query) : 0 };

                // 分页响应：当前页条目 + 符合条件的总数；非分页时直接返回数组
                if (page_size > 0)
                {
                    json["items"]     = nlohmann::json::array();
                    json["total"]     = total;
                    json["page"]      = page;
                    json["page_size"] = page_size;
                }
                else
                {
                    json = nlohmann::json::array();
                }
                nlohmann::json& items = page_size > 0 ? json["items"] : json;

                auto blogs = doc::get_blogs(conn, query);
                for (const auto& b : blogs)
                {
                    nlohmann::json item;
                    item["id"]          = b.id;
                    item["title"]       = b.title;
                    item["description"] = b.description.has_value()
                                        ? nlohmann::json(*b.description)
                                        : nlohmann::json(nullptr);
                    item["update_time"] = b.update_time;
                    item["categories"]  = b.categories;
                    item["tags"]        = b.tags;
                    item["file_path"]   = b.file_path.has_value()
                                        ? nlohmann::json(*b.file_path)
                                        : nlohmann::json(nullptr);
                    items.push_back(std::move(item));
                }

                spdlog::debug("博客列表从数据库获取成功。");

                res.set_content(json.dump(), "application/json");

                if (!blogs.empty())
                {
                    cache_set_list(key, res.body, std::stoll(config::config()["CACHE_TTL_BLOGS"]));

                    spdlog::debug("非空博客列表已加入缓存。");
                }
            }
        );
        if (err)
        {
            spdlog::warn("获取博客列表失败：{}", *err);
            res.status = 503;
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            json["error"] = "数据库繁忙，请稍后重试";
            res.set_content(json.dump(), "application/json");
        }
    }

    void handle_get_blog(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        spdlog::debug("收到博客详情获取请求。");

        nlohmann::json json;

        if (!req.has_param("file_path"))
        {
            spdlog::debug("获取博客详情失败：缺少 file_path 参数。");

            res.status = 400;

            json["error"] = "缺少 file_path 参数";
            res.set_content(json.dump(), "application/json");
            return;
        }

        const auto file_path = req.get_param_value("file_path");
        const auto key = cache::cache_key("/api/blog", { { "file_path", file_path } });
        if (const auto cached = cache::get(key); cached.has_value())
        {
            spdlog::debug("博客详情从缓存获取成功：{}。", file_path);

            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(*cached, "application/json");
            return;
        }

        const auto err = db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                auto blog = doc::get_blog_by_file_path(conn, file_path);
                if (!blog)
                {
                    spdlog::debug("获取博客失败：{} 不存在。", file_path);

                    res.status = 404;

                    json["error"] = "博客不存在";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                json["id"]          = blog->id;
                json["title"]       = blog->title;
                json["description"] = blog->description;
                json["content"]     = blog->content;
                json["update_time"] = blog->update_time;
                json["categories"]  = blog->categories;
                json["file_path"]   = blog->file_path;
                json["tags"]        = blog->tags;

                spdlog::debug("博客详情获取成功：{}。", file_path);

                res.set_content(json.dump(), "application/json");

                cache::set(key, res.body, std::stoll(config::config()["CACHE_TTL_BLOG"]));

                spdlog::debug("博客详情已写入缓存：{}。", file_path);
            }
        );
        if (err)
        {
            spdlog::warn("获取博客详情失败：{}", *err);
            res.status = 503;
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            json["error"] = "数据库繁忙，请稍后重试";
            res.set_content(json.dump(), "application/json");
        }
    }

    void handle_blog_parse(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        spdlog::debug("收到 md 文件解析请求。");

        nlohmann::json json;

        res.set_header("Access-Control-Allow-Origin", allowed);
        res.set_header("Content-Type", "application/json");

        json = md::parse_frontmatter(req.body);

        spdlog::debug("md 文件解析请求完成。");

        res.set_content(json.dump(), "application/json");
    }

} // namespace http
