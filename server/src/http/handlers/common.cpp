/**
 * @file http/handlers/common.cpp
 * @brief 公共 HTTP 路由处理函数与跨模块共用工具实现
 */

#include "http/handlers/common.h"

#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "cache/cache.h"
#include "db/connection_pool.h"

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

    void cache_set_list(
        const std::string& key,
        const std::string& body,
        long long          ttl)
    {
        if (body != "[]")
            cache::set(key, body, ttl);
    }

    void with_db(
        httplib::Response& res,
        const std::string& allowed,
        std::string_view   action,
        const std::function<void(pqxx::connection&)>& fn)
    {
        if (const auto err = db::with_db(fn))
        {
            spdlog::warn("{}失败：{}", action, *err);

            res.status = 503;
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");

            nlohmann::json json;
            json["error"] = "数据库繁忙，请稍后重试";
            res.set_content(json.dump(), "application/json");
        }
    }

} // namespace http
