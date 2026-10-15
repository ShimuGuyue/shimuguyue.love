/**
 * @file http/handlers/common.h
 * @brief 公共 HTTP 路由处理函数与跨模块共用工具声明
 */
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include <httplib.h>
#include <pqxx/pqxx>

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

    /**
     * @brief 获取一条独占数据库连接执行回调；取用失败时记录日志并统一返回 503。
     * @param res     响应对象，取用失败时写入 503 状态与 CORS 头。
     * @param allowed CORS 允许的前端来源。
     * @param action  要执行的操作，最终失败日志为「{action}失败：{错误信息}」。
     * @param fn      数据库回调，参数为可用的独占连接。
     */
    void with_db(
        httplib::Response&                            res,
        const std::string&                            allowed,
        std::string_view                              action,
        const std::function<void(pqxx::connection&)>& fn);

} // namespace http
