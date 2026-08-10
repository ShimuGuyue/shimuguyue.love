/**
 * @file http/routes.cpp
 * @brief HTTP 路由注册
 */

#include "http/routes.h"

#include <filesystem>
#include <string>

#include "config/env.h"
#include "http/handlers.h"

namespace http
{
    void setup_routes(httplib::Server& svr)
    {
        const std::string allowed = config::env()["FRONTEND_ORIGIN"];

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

        // 挂载图片静态文件服务（图片目录为 FILE_PATH/image）
        svr.set_mount_point("/image",
            (std::filesystem::path{ config::env()["FILE_PATH"] } / "image").string());

        // GET /api/images — 获取所有图片
        svr.Get("/api/images",
            [allowed](const auto& req, auto& res)
            {
                handle_get_images(req, res, allowed);
            }
        );

        // POST /api/image/save — 保存/更新图片元数据（需要 edit 权限）
        svr.Post("/api/image/save",
            [allowed](const auto& req, auto& res)
            {
                handle_save_image(req, res, allowed);
            }
        );

        // POST /api/image/upload — 上传图片文件（需要 edit 权限）
        svr.Post("/api/image/upload",
            [allowed](const auto& req, auto& res)
            {
                handle_upload_image(req, res, allowed);
            }
        );

        // DELETE /api/image/delete — 删除图片（需要 edit 权限）
        svr.Delete("/api/image/delete",
            [allowed](const auto& req, auto& res)
            {
                handle_delete_image(req, res, allowed);
            }
        );

        // POST /api/blog/save — 保存博客
        svr.Post("/api/blog/save",
            [allowed](const auto& req, auto& res)
            {
                handle_save_blog(req, res, allowed);
            }
        );

        // PUT /api/blog/update — 编辑已有博客
        svr.Put("/api/blog/update",
            [allowed](const auto& req, auto& res)
            {
                handle_update_blog(req, res, allowed);
            }
        );

        // DELETE /api/blog/delete — 删除博客
        svr.Delete("/api/blog/delete",
            [allowed](const auto& req, auto& res)
            {
                handle_delete_blog(req, res, allowed);
            }
        );

        // GET /api/about — 获取《关于我》README 内容（从数据库读取）
        svr.Get("/api/about",
            [allowed](const auto& req, auto& res)
            {
                handle_get_about(req, res, allowed);
            }
        );

        // GET /api/profile — 获取个人介绍
        svr.Get("/api/profile",
            [allowed](const auto& req, auto& res)
            {
                handle_get_profile(req, res, allowed);
            }
        );

        // POST /api/profile/save — 更新个人介绍（需要 edit 权限）
        svr.Post("/api/profile/save",
            [allowed](const auto& req, auto& res)
            {
                handle_save_profile(req, res, allowed);
            }
        );
    }

} // namespace http
