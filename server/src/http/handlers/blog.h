/**
 * @file http/handlers/blog.h
 * @brief 博客公开查询 HTTP 路由处理函数声明
 */

#pragma once

#include <string>

#include <httplib.h>

namespace http
{
    /**
     * @brief 处理 GET /api/categories 请求。
     * @details 博客分类列表获取。
     */
    void handle_get_categories(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 GET /api/tags 请求。
     * @details 博客标签列表获取。
     */
    void handle_get_tags(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 GET /api/blogs 请求。
     * @details 博客列表获取。
     */
    void handle_get_blogs(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 GET /api/blog 请求。
     * @details 根据路径获取指定博客信息。
     */
    void handle_get_blog(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

    /**
     * @brief 处理 POST /api/blog/parse 请求。
     */
    void handle_blog_parse(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed);

} // namespace http
