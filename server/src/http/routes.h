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
     * @brief 获取环境变量 FRONTEND_ORIGIN。
     * @return 合法的环境变量 FRONTEND_ORIGIN。
     */
    auto frontend_origin() -> std::string;

    /**
     * @brief 获取环境变量 SERVER_HOST。
     * @return 合法的环境变量 SERVER_HOST。
     */
    auto server_host() -> std::string;

    /**
     * @brief 获取环境变量 SERVER_PORT。
     * @return 合法的环境变量 SERVER_PORT。
     */
    auto server_port() -> int;

    /**
     * @brief 从环境变量初始化所有所需变量。
     *        未设置或无效则打印错误并调用 std::exit(1)。
     */
    void init();

    /**
     * @brief 注册所有 API 路由到 HTTP 服务器。
     * @param svr  httplib::Server 实例。
     * @param conn 数据库连接。
     */
    void setup_routes(httplib::Server& svr, pqxx::connection& conn);

} // namespace http
