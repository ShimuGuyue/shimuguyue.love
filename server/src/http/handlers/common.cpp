/**
 * @file http/handlers/common.cpp
 * @brief 公共 HTTP 路由处理函数与跨模块共用工具实现
 */

#include "http/handlers/common.h"

#include <string>

#include "cache/cache.h"

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

} // namespace http
