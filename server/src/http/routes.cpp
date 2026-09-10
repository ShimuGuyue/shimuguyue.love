/**
 * @file http/routes.cpp
 * @brief HTTP 路由注册
 */

#include "http/routes.h"

#include <filesystem>
#include <string>

#include "config/config.h"
#include "http/handlers.h"

namespace http
{
    void setup_routes(httplib::Server& svr)
    {
        const std::string allowed = config::config()["FRONTEND_ORIGIN"];

        // 挂载静态文件服务:
        // 照片墙目录为 FILE_PATH/photo_wall，对外访问路径与目录同名
        svr.set_mount_point("/photo_wall",
            (std::filesystem::path{ config::config()["FILE_PATH"] } / "photo_wall").string());

        svr.Options("/api/.*",
            [allowed](const auto& req, auto& res)
            {
                handle_cors(req, res, allowed);
            }
        );

        svr.Post("/api/login/key",
            [allowed](const auto& req, auto& res)
            {
                handle_login_key(req, res, allowed);
            }
        );

        svr.Post("/api/login/password",
            [allowed](const auto& req, auto& res)
            {
                handle_login_password(req, res, allowed);
            }
        );

        svr.Get("/api/user/permissions",
            [allowed](const auto& req, auto& res)
            {
                handle_user_permissions(req, res, allowed);
            }
        );

        // GET /api/user/info — 获取当前登录用户信息（保存后一次性刷新导航栏用户名）
        svr.Get("/api/user/info",
            [allowed](const auto& req, auto& res)
            {
                handle_user_info(req, res, allowed);
            }
        );

        // POST /api/user/update — 用户自助更新自己的信息（用户名 / 密钥可用状态 / 密码）
        svr.Post("/api/user/update",
            [allowed](const auto& req, auto& res)
            {
                handle_user_update(req, res, allowed);
            }
        );

        // GET /api/manage/users — 获取用户列表（需要 manage:view 权限）
        svr.Get("/api/manage/users",
            [allowed](const auto& req, auto& res)
            {
                handle_manage_users(req, res, allowed);
            }
        );

        // POST /api/manage/user/update — 更新用户（需要 manage:edit 权限）
        svr.Post("/api/manage/user/update",
            [allowed](const auto& req, auto& res)
            {
                handle_manage_update_user(req, res, allowed);
            }
        );

        // POST /api/manage/user/create — 新建用户（需要 manage:edit 权限）
        svr.Post("/api/manage/user/create",
            [allowed](const auto& req, auto& res)
            {
                handle_manage_create_user(req, res, allowed);
            }
        );

        // GET /api/manage/download — 下载数据表 zip（需要 manage:download 权限）
        svr.Get("/api/manage/download",
            [allowed](const auto& req, auto& res)
            {
                handle_manage_download(req, res, allowed);
            }
        );

        svr.Get("/api/categories",
            [allowed](const auto& req, auto& res)
            {
                handle_get_categories(req, res, allowed);
            }
        );

        svr.Get("/api/tags",
            [allowed](const auto& req, auto& res)
            {
                handle_get_tags(req, res, allowed);
            }
        );

        svr.Get("/api/blogs",
            [allowed](const auto& req, auto& res)
            {
                handle_get_blogs(req, res, allowed);
            }
        );

        svr.Get("/api/blog",
            [allowed](const auto& req, auto& res)
            {
                handle_get_blog(req, res, allowed);
            }
        );

        // POST /api/blog/parse — 解析 Markdown frontmatter (委托 md::parse_frontmatter)
        svr.Post("/api/blog/parse",
            [allowed](const auto& req, auto& res)
            {
                handle_blog_parse(req, res, allowed);
            }
        );

        // GET /api/images — 获取所有图片
        svr.Get("/api/images",
            [allowed](const auto& req, auto& res)
            {
                handle_get_images(req, res, allowed);
            }
        );

        // POST /api/image/save — 保存/更新图片元数据（需要 photo_wall:edit 权限）
        svr.Post("/api/image/save",
            [allowed](const auto& req, auto& res)
            {
                handle_save_image(req, res, allowed);
            }
        );

        // POST /api/image/upload — 上传图片文件（需要 photo_wall:upload 权限）
        svr.Post("/api/image/upload",
            [allowed](const auto& req, auto& res)
            {
                handle_upload_image(req, res, allowed);
            }
        );

        // DELETE /api/image/delete — 删除图片（需要 photo_wall:delete 权限）
        svr.Delete("/api/image/delete",
            [allowed](const auto& req, auto& res)
            {
                handle_delete_image(req, res, allowed);
            }
        );

        // POST /api/blog/save — 新建博客（需要 blog:create 权限）
        svr.Post("/api/blog/save",
            [allowed](const auto& req, auto& res)
            {
                handle_save_blog(req, res, allowed);
            }
        );

        // PUT /api/blog/update — 编辑已有博客（需要 blog:edit 权限）
        svr.Put("/api/blog/update",
            [allowed](const auto& req, auto& res)
            {
                handle_update_blog(req, res, allowed);
            }
        );

        // DELETE /api/blog/delete — 删除博客（需要 blog:delete 权限）
        svr.Delete("/api/blog/delete",
            [allowed](const auto& req, auto& res)
            {
                handle_delete_blog(req, res, allowed);
            }
        );

        // GET /api/blog/download — 下载博客 .md 文件（需要 blog:download 权限）
        svr.Get("/api/blog/download",
            [allowed](const auto& req, auto& res)
            {
                handle_download_blog(req, res, allowed);
            }
        );

    }

} // namespace http
