/**
 * @file http/handlers/image.cpp
 * @brief 照片墙图片 HTTP 路由处理函数实现
 */

#include "http/handlers/image.h"

#include <algorithm>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "auth/session.h"
#include "cache/cache.h"
#include "config/config.h"
#include "db/connection_pool.h"
#include "http/handlers/common.h"
#include "img/image_queries.h"

namespace http
{
    void handle_get_images(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "照片墙信息获取";
        spdlog::debug("收到 {} 请求。", ACTION);

        nlohmann::json json;

        const auto key = cache::cache_key("/api/images", {});
        if (const auto cached = cache::get(key); cached.has_value())
        {
            res.set_header("Access-Control-Allow-Origin", allowed);
            res.set_header("Content-Type", "application/json");
            res.set_content(*cached, "application/json");

            spdlog::debug("照片墙信息从缓存获取成功。");
            return;
        }

        with_db(res, allowed, ACTION,
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");
                json = img::get_all_images(conn);
                res.set_content(json.dump(), "application/json");

                spdlog::debug("照片墙信息从数据库获取成功。");

                cache_set_list(key, res.body, std::stoll(config::config()["CACHE_TTL_IMAGES"]));

                spdlog::info("照片墙信息已加入缓存。");
            }
        );
    }

    void handle_save_image(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "图片元数据保存";
        spdlog::info("收到 {} 请求。", ACTION);

        nlohmann::json json;

        with_db(res, allowed, ACTION,
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

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

                const auto& permissions = session->permissions;
                if (std::find(permissions.begin(), permissions.end(), "photo_wall:edit") == permissions.end())
                {
                    spdlog::info("{}失败：用户 {} 无 photo_wall:edit 权限。", ACTION, session->user_id);

                    res.status = 403;

                    json["error"] = "当前用户无 photo_wall:edit 权限";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("{}失败：无效的 JSON。", ACTION);

                    res.status = 400;

                    json["error"] = "无效的 JSON";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                if (auto err = img::save_image(
                        conn,
                        body.value("path", ""),
                        body.value("description", ""),
                        body.value("scale", 1.0),
                        body.value("rotation", 0.0),
                        body.value("pos_x", 50.0),
                        body.value("pos_y", 50.0),
                        body.value("z", 0)
                    )
                )
                {
                    spdlog::error("{}失败：{}", ACTION, *err);

                    res.status = 500;

                    json["error"] = *err;
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                spdlog::info("{}成功：{}", ACTION, body.value("path", ""));

                json["ok"] = true;
                res.set_content(json.dump(), "application/json");

                // 删除旧缓存
                cache::invalidate_prefix("api-cache:/api/images");
            }
        );
    }

    void handle_upload_image(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "图片上传";
        spdlog::info("收到 {} 请求。", ACTION);

        nlohmann::json json;

        with_db(res, allowed, ACTION,
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

                if (!req.form.has_file("file"))
                {
                    spdlog::info("{}失败：未选择文件。", ACTION);

                    res.status = 400;

                    json["error"] = "未选择文件";
                    res.set_content(json.dump(), "application/json");
                    return;
                }
                const auto file = req.form.get_file("file");

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

                const auto& permissions = session->permissions;
                if (std::find(permissions.begin(), permissions.end(), "photo_wall:upload") == permissions.end())
                {
                    spdlog::info("{}失败：用户 {} 无 photo_wall:upload 权限。", ACTION, session->user_id);

                    res.status = 403;

                    json["error"] = "当前用户无 photo_wall:upload 权限";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                auto [err, result] = img::upload_image(conn, file.filename, file.content);
                if (err)
                {
                    spdlog::error("{}失败：{}", ACTION, *err);
                    res.status = 500;

                    json["error"] = *err;
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                spdlog::info("{}成功：{}。", ACTION, file.filename);

                json = result;
                res.set_content(json.dump(), "application/json");

                // 删除旧缓存
                cache::invalidate_prefix("api-cache:/api/images");
            }
        );
    }

    void handle_delete_image(
        const httplib::Request& req,
        httplib::Response&      res,
        const std::string&      allowed)
    {
        constexpr std::string_view ACTION = "图片删除";
        spdlog::info("收到 {} 请求。", ACTION);

        nlohmann::json json;

        with_db(res, allowed, ACTION,
            [&](pqxx::connection& conn)
            {
                res.set_header("Access-Control-Allow-Origin", allowed);
                res.set_header("Content-Type", "application/json");

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

                const auto& permissions = session->permissions;
                if (std::find(permissions.begin(), permissions.end(), "photo_wall:delete") == permissions.end())
                {
                    spdlog::info("{}失败：用户 {} 无 photo_wall:delete 权限。", ACTION, session->user_id);

                    res.status = 403;

                    json["error"] = "当前用户无 photo_wall:delete 权限";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                const auto body = nlohmann::json::parse(req.body, nullptr, false);
                if (body.is_discarded())
                {
                    spdlog::info("{}失败：无效的 JSON。", ACTION);

                    res.status = 400;

                    json["error"] = "无效的 JSON";
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                const auto path = body.value("path", "");
                if (auto err = img::delete_image(conn, path))
                {
                    spdlog::error("{}失败：{}", ACTION, *err);

                    res.status = 500;

                    json["error"] = *err;
                    res.set_content(json.dump(), "application/json");
                    return;
                }

                spdlog::info("{}成功：{}。", ACTION, path);

                json["ok"] = true;
                res.set_content(json.dump(), "application/json");

                // 删除旧缓存
                cache::invalidate_prefix("api-cache:/api/images");
            }
        );
    }

} // namespace http
