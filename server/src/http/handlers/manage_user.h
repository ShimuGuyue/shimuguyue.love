/**
 * @file http/handlers/manage_user.h
 * @brief 后台用户管理 HTTP 路由处理函数声明
 */

#pragma once

#include <string>

#include <httplib.h>

namespace http
{

/**
 * @brief 处理 GET /api/manage/users 请求（需 manage:view 权限）。
 */
void handle_manage_users(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/manage/user/update 请求（需 manage:edit 权限）。
 */
void handle_manage_update_user(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/manage/user/create 请求（需 manage:edit 权限）。
 */
void handle_manage_create_user(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

} // namespace http
