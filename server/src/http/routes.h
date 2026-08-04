/**
 * @file http/routes.h
 * @brief HTTP 路由注册
 */
#pragma once

#include <pqxx/pqxx>

#include <httplib.h>

namespace http
{
    /**
     * @brief 注册所有 API 路由到 HTTP 服务器。
     * @param svr  httplib::Server 实例。
     * @param conn 数据库连接。
     */
    void setup_routes(httplib::Server& svr, pqxx::connection& conn);

} // namespace http
