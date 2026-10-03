/**
 * @file http/handlers/common.h
 * @brief 公共 HTTP 路由处理函数与跨模块共用工具声明
 */
#pragma once

#include <string>

#include <httplib.h>

namespace http
{

/**
 * @brief 处理 CORS 预检请求。
 */
void handle_cors(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 缓存列表响应；空数组不缓存，避免空结果长期滞留导致页面空白。
 * @param key  缓存键。
 * @param body 响应体。
 * @param ttl  过期秒数。
 */
void cache_set_list(
    const std::string& key,
    const std::string& body,
    long long          ttl);

} // namespace http
