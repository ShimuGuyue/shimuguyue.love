/**
 * @file http/handlers.h
 * @brief HTTP 路由处理函数声明
 */
#pragma once

#include <string>

#include <httplib.h>

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
 * @brief 处理 POST /api/login/key 请求。
 */
void handle_login_key(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/login/password 请求。
 */
void handle_login_password(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/user/permissions 请求。
 */
void handle_user_permissions(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/user/info 请求。
 */
void handle_user_info(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/user/update 请求（用户自助更新自己的信息）。
 */
void handle_user_update(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

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

/**
 * @brief 处理 GET /api/manage/download 请求（需 manage:download 权限）。
 */
void handle_manage_download(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/categories 请求。
 */
void handle_get_categories(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/tags 请求。
 */
void handle_get_tags(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/blogs 请求。
 */
void handle_get_blogs(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/blog 请求。
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

/**
 * @brief 处理 GET /api/images 请求。
 */
void handle_get_images(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/friends 请求。
 */
void handle_get_friends(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/friends 请求（需 manage:edit 权限）。
 */
void handle_create_friend(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 PUT /api/friends/update 请求（需 manage:edit 权限）。
 */
void handle_update_friend(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/friend/avatar/upload 请求（需 manage:edit 权限）。
 */
void handle_upload_avatar(
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

/**
 * @brief 处理 GET /api/about 请求。
 */
void handle_get_about(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 GET /api/profile 请求。
 */
void handle_get_profile(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

/**
 * @brief 处理 POST /api/profile/save 请求（需 introduction:edit 权限）。
 */
void handle_save_profile(
    const httplib::Request& req,
    httplib::Response&      res,
    const std::string&      allowed);

} // namespace http
