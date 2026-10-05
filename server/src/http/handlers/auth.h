/**
 * @file http/handlers/auth.h
 * @brief 登录认证与当前用户信息 HTTP 路由处理函数声明
 */
#pragma once

#include <string>

#include <httplib.h>

namespace http
{
    /**
     * @brief 处理 POST /api/login/key 请求。
     * @details 使用密钥进行登录。
     */
    void handle_login_key(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 POST /api/login/password 请求。
     * @details 使用用户名和密码进行登录。
     */
    void handle_login_password(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 GET /api/user/permissions 请求。
     * @details 获取当前登录用户自身的权限。
     */
    void handle_user_permissions(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 GET /api/user/info 请求。
     * @details 获取当前登录用户自身的信息。
     */
    void handle_user_info(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 POST /api/user/update 请求。
     * @details 自助修改当前登录用户自身的信息。
     */
    void handle_user_update(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

} // namespace http
