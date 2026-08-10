/**
 * @file http/routes.h
 * @brief HTTP 路由注册
 */
#pragma once

#include <httplib.h>

namespace http
{
    /**
     * @brief 注册所有 API 路由到 HTTP 服务器。
     * @param svr httplib::Server 实例。
     */
    void setup_routes(httplib::Server& svr);

} // namespace http
