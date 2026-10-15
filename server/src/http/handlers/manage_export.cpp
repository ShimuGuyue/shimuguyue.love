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
#include "http/handlers/common.h"

namespace http
{
    void handle_manage_download(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "数据下载";
        spdlog::info("收到 {} 请求。", ACTION);

        nlohmann::json json;

        with_db(res, allowed, ACTION,
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
                    spdlog::info("{}失败：未登录或会话已过期。", ACTION);

                    res.status = 401;

                    json["error"] = "未登录或会话已过期";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 导出范围：blogs = 博客/分类/标签，users = 用户/权限/用户权限关联
                const auto scope = req.get_param_value("scope");
                if (scope != "blogs" && scope != "users")
                {
                    spdlog::info("{}失败：无效的导出范围 {}", ACTION, scope);

                    res.status = 400;

                    json["error"] = "无效的导出范围";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 权限检查：manage:download
                const auto& permissions = session->permissions;
                if (std::find(permissions.begin(), permissions.end(), "manage:download") == permissions.end())
                {
                    spdlog::info("{}失败：用户 {} 无 manage:download 权限。", ACTION, session->user_id);

                    res.status = 403;

                    json["error"] = "当前用户无 manage:download 权限";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                // 调用数据打包逻辑
                std::expected<std::string, std::string> zip;
                if (scope == "blogs")
                    zip = export_data::build_blogs_export_zip(conn);
                else if (scope == "users")
                    zip = export_data::build_users_export_zip(conn);

                if (!zip)
                {
                    spdlog::error("{}失败：{}", ACTION, zip.error());

                    res.status = 500;

                    json["error"] = zip.error();
                    res.set_content(json.dump(), "application/json");
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

                spdlog::info("{}成功。", ACTION);
            }
        );
    }

} // namespace http
