/**
 * @file http/handlers/image.h
 * @brief 照片墙图片 HTTP 路由处理函数声明
 */

#pragma once

#include <string>

#include <httplib.h>

namespace http
{

/**
 * @brief 处理 GET /api/images 请求。
 */
void handle_get_images(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/image/save 请求（需 photo_wall:edit 权限）。
 */
void handle_save_image(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/image/upload 请求（需 photo_wall:upload 权限）。
 */
void handle_upload_image(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 DELETE /api/image/delete 请求（需 photo_wall:delete 权限）。
 */
void handle_delete_image(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

} // namespace http
