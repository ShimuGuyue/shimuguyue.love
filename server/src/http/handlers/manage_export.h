/**
 * @file http/handlers/manage_export.h
 * @brief 后台数据导出 HTTP 路由处理函数声明
 */

#pragma once

#include <string>

#include <httplib.h>

namespace http
{
    /**
     * @brief 处理 GET /api/manage/download 请求（需 manage:download 权限）。
     * @details 后台数据打包导出。
     */
    void handle_manage_download(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

} // namespace http
