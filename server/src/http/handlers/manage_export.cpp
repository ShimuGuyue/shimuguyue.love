/**
 * @file http/handlers/manage_export.cpp
 * @brief 后台数据导出 HTTP 路由处理函数实现
 */

#include "http/handlers/manage_export.h"

#include <algorithm>
#include <ctime>
#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "auth/session.h"
#include "db/connection_pool.h"
#include "export/export_data.h"

namespace http
{
    void handle_manage_download(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        db::with_db(
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/zip");

                // Session 验证
                std::string token;  // 提取 Bearer token
                if (req.has_header("Authorization"))
                {
                    const auto& auth_hdr = req.get_header_value("Authorization");
                    constexpr std::string_view PREFIX = "Bearer ";
                    if (auth_hdr.size() > PREFIX.size()
                    &&  auth_hdr.compare(0, PREFIX.size(), PREFIX) == 0)
                        token = auth_hdr.substr(PREFIX.size());
                }
                const auto session = auth::validate_session(conn, token);
                if (!session)
                {
                    spdlog::info("数据下载失败：未登录或会话已过期。");
                    res.status = 401;
                    res.set_content(R"({"error":"未登录或会话已过期"})", "application/json");
                    return;
                }

                // 导出范围：blogs = 博客/分类/标签，users = 用户/权限/用户权限关联
                const auto scope = req.get_param_value("scope");
                if (scope != "blogs" && scope != "users")
                {
                    spdlog::info("数据下载失败：无效的导出范围 {}", scope);
                    res.status = 400;
                    res.set_content(R"({"error":"无效的导出范围"})", "application/json");
                    return;
                }

                // 权限检查：数据下载需要 manage:download 权限
                const auto& perms = session->permissions;
                if (std::find(perms.begin(), perms.end(), "manage:download") == perms.end())
                {
                    spdlog::info("数据下载失败：用户 {} 无 manage:download 权限。", session->user_id);
                    res.status = 403;
                    res.set_content(R"({"error":"当前用户无 manage:download 权限"})", "application/json");
                    return;
                }

                std::expected<std::string, std::string> zip;
                if (scope == "blogs")
                {
                    zip = export_data::build_blogs_export_zip(conn);
                }
                else
                {
                    zip = export_data::build_users_export_zip(conn);
                }
                if (!zip)
                {
                    spdlog::error("数据下载失败：{}", zip.error());
                    res.status = 500;
                    res.set_content(
                        nlohmann::json{{"error", zip.error()}}.dump(),
                        "application/json"
                    );
                    return;
                }

                // 附件文件名带上导出日期
                std::time_t now{ std::time(nullptr) };
                char date_buf[16];
                std::strftime(date_buf, sizeof date_buf, "%Y%m%d", std::localtime(&now));
                res.set_header(
                    "Content-Disposition",
                    "attachment; filename=\"data-" + scope + "-" + std::string{ date_buf } + ".zip\""
                );
                res.set_content(zip->data(), zip->size(), "application/zip");
                spdlog::info("数据下载成功。");
            }
        );
    }

} // namespace http
