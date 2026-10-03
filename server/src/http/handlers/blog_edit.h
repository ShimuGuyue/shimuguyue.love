/**
 * @file http/handlers/blog_edit.h
 * @brief 博客新建 / 编辑 / 删除 / 下载 HTTP 路由处理函数声明
 */

#pragma once

#include <string>

#include <httplib.h>

namespace http
{

/**
 * @brief 处理 POST /api/blog/save 请求（需 blog:create 权限）。
 */
void handle_save_blog(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 PUT /api/blog/update 请求（需 blog:edit 权限）。
 */
void handle_update_blog(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 DELETE /api/blog/delete 请求（需 blog:delete 权限）。
 */
void handle_delete_blog(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/blog/download 请求（需 blog:download 权限）。
 */
void handle_download_blog(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

} // namespace http
