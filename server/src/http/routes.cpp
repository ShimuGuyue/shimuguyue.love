/**
 * @file http/routes.cpp
 * @brief HTTP 路由注册
 */

#include "http/routes.h"

#include <string>

#include "config/env.h"
#include "http/handlers.h"

namespace http
{
    void setup_routes(httplib::Server& svr, pqxx::connection& conn)
    {
        const std::string allowed = config::env()["FRONTEND_ORIGIN"];

        svr.Options("/api/.*",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_cors(req, res, conn, allowed);
            }
        );

        svr.Post("/api/login/key",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_login_key(req, res, conn, allowed);
            }
        );

        svr.Post("/api/login/password",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_login_password(req, res, conn, allowed);
            }
        );

        svr.Get("/api/user/permissions",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_user_permissions(req, res, conn, allowed);
            }
        );

        svr.Get("/api/categories",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_get_categories(req, res, conn, allowed);
            }
        );

        svr.Get("/api/tags",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_get_tags(req, res, conn, allowed);
            }
        );

        svr.Get("/api/blogs",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_get_blogs(req, res, conn, allowed);
            }
        );

        svr.Get("/api/blog",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_get_blog(req, res, conn, allowed);
            }
        );

        // POST /api/blog/parse — 解析 Markdown frontmatter (委托 md::parse_frontmatter)
        svr.Post("/api/blog/parse",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_blog_parse(req, res, conn, allowed);
            }
        );

        // 挂载图片静态文件服务
        svr.set_mount_point("/image", config::env()["IMAGE_PATH"]);

        // GET /api/images — 获取所有图片
        svr.Get("/api/images",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_get_images(req, res, conn, allowed);
            }
        );

        // POST /api/image/save — 保存/更新图片元数据（需要 edit 权限）
        svr.Post("/api/image/save",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_save_image(req, res, conn, allowed);
            }
        );

        // POST /api/image/upload — 上传图片文件（需要 edit 权限）
        svr.Post("/api/image/upload",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_upload_image(req, res, conn, allowed);
            }
        );

        // DELETE /api/image/delete — 删除图片（需要 edit 权限）
        svr.Delete("/api/image/delete",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_delete_image(req, res, conn, allowed);
            }
        );

        // POST /api/blog/save — 保存博客
        svr.Post("/api/blog/save",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_save_blog(req, res, conn, allowed);
            }
        );

        // PUT /api/blog/update — 编辑已有博客
        svr.Put("/api/blog/update",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_update_blog(req, res, conn, allowed);
            }
        );

        // DELETE /api/blog/delete — 删除博客
        svr.Delete("/api/blog/delete",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_delete_blog(req, res, conn, allowed);
            }
        );

        // GET /api/about — 获取《关于我》README 内容（从数据库读取）
        svr.Get("/api/about",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_get_about(req, res, conn, allowed);
            }
        );

        // GET /api/profile — 获取个人介绍
        svr.Get("/api/profile",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_get_profile(req, res, conn, allowed);
            }
        );

        // POST /api/profile/save — 更新个人介绍（需要 edit 权限）
        svr.Post("/api/profile/save",
            [&conn, allowed](const auto& req, auto& res)
            {
                handle_save_profile(req, res, conn, allowed);
            }
        );
    }

} // namespace http
