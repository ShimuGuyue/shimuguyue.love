/**
 * @file image/image_queries.cpp
 * @brief 照片墙图片数据库查询实现
 */

#include "img/image_queries.h"


#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "config/env.h"

namespace img
{

    void init()
    {
        // 确保子目录存在
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path{ config::env()["FILE_PATH"] } / "image" / "home", ec);
    }

    auto get_all_images(pqxx::connection& conn) -> nlohmann::json
    {
        spdlog::debug("正在获取照片墙图片信息...");
        pqxx::work txn{ conn };
        const auto rows = txn.exec(
            "SELECT id, path, description, scale, rotation, pos_x, pos_y, z "
            "FROM images "
            "ORDER BY id"
        );
        txn.commit();

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            nlohmann::json item;
            item["id"]          = row["id"]         .as<int>();
            item["path"]        = row["path"]       .as<std::string>();
            item["description"] = row["description"].as<std::string>();
            item["scale"]       = row["scale"]      .as<double>();
            item["rotation"]    = row["rotation"]   .as<double>();
            item["pos_x"]       = row["pos_x"]      .as<double>();
            item["pos_y"]       = row["pos_y"]      .as<double>();
            item["z"]           = row["z"]          .as<int>();
            arr.push_back(std::move(item));
        }
        spdlog::info("获取照片墙图片信息完成。");
        return arr;
    }

    auto save_image(
        pqxx::connection& conn,
        std::string_view  path,
        std::string_view  description,
        double            scale,
        double            rotation,
        double            pos_x,
        double            pos_y,
        int               z)
    -> std::optional<std::string>
    {
        spdlog::debug("正在更新照片墙图片信息...");
        pqxx::work txn{ conn };
        txn.exec(
            "UPDATE images SET "
            "description = $1, "
            "scale       = $2, "
            "rotation    = $3, "
            "pos_x       = $4, "
            "pos_y       = $5, "
            "z           = $6 "
            "WHERE path = $7",
            pqxx::params{
                std::string{ description },
                scale,
                rotation,
                pos_x,
                pos_y,
                z,
                std::string{ path }
            }
        );
        txn.commit();
        spdlog::info("更新照片墙图片信息完成。");
        return std::nullopt;
    }

    auto delete_image(
        pqxx::connection& conn,
        std::string_view  path)
    -> std::optional<std::string>
    {
        spdlog::debug("正在删除照片墙图片信息...");
        pqxx::work txn{ conn };
        const auto r = txn.exec(
            "DELETE FROM images WHERE path = $1", pqxx::params{ std::string{path} }
        );
        txn.commit();

        if (r.affected_rows() == 0)
        {
            spdlog::info("照片记录不存在。");
            return std::string{ "图片记录不存在" };
        }

        std::error_code ec;
        std::filesystem::path file_path{
            std::filesystem::path{ config::env()["FILE_PATH"] } / "image" / std::string{ path }
        };
        if (!std::filesystem::remove(file_path, ec) && ec)
            spdlog::error("删除文件失败: {} - {}", file_path.string(), ec.message());

        spdlog::info("删除照片墙图片信息完成。");
        return std::nullopt;
    }

    auto upload_image(
        pqxx::connection& conn,
        std::string_view  filename,
        std::string_view  data)
    -> std::pair<std::optional<std::string>, nlohmann::json>
    {
        spdlog::debug("正在上传照片墙图片...");
        // 校验扩展名
        const auto ext_pos = filename.rfind('.');
        if (ext_pos == std::string::npos)
        {
            spdlog::debug("上传失败：文件缺少拓展名。");
            return { std::string{ "文件缺少扩展名" }, {} };
        }
        const auto ext = filename.substr(ext_pos);
        if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" && ext != ".gif" && ext != ".webp" && ext != ".svg")
        {
            spdlog::debug("上传失败：不支持的文件格式。");
            return { std::string{ "不支持的文件格式" }, {} };
        }

        // 插入数据库获取 id
        spdlog::debug("正在将图片信息上传至数据库...");
        std::string rel_path;
        int image_id{ 0 };
        {
            pqxx::work txn{ conn };
            const auto r = txn.exec(
                "INSERT INTO images (path, description, scale, rotation, pos_x, pos_y) "
                "VALUES ('', '', 1.0, 0.0, 50.0, 50.0) RETURNING id"
            );
            image_id = r[0]["id"].as<int>();
            rel_path = std::format("home/{}{}", image_id, ext);
            txn.exec("UPDATE images SET path = $1 WHERE id = $2",
                     pqxx::params{ rel_path, image_id });
            txn.commit();
        }
        spdlog::debug("将图片信息上传至数据库完成。");

        // 写入文件
        spdlog::debug("正在将图片文件写入目录...");
        const auto full =
            (std::filesystem::path{ config::env()["FILE_PATH"] } / "image" / rel_path).string();
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(full).parent_path(), ec);
        if (ec)
        {
            spdlog::error("创建目录失败: {} - {}", full, ec.message());
            return { std::string{ "创建目录失败" }, {} };
        }
        std::ofstream ofs{ full, std::ios::binary };
        if (!ofs)
        {
            spdlog::error("打开文件失败: {}", full);
            return { std::string{ "写入文件失败" }, {} };
        }
        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!ofs) {
            spdlog::error("写入文件失败: {}", full);
            return { std::string{ "写入文件失败" }, {} };
        }
        ofs.close();
        spdlog::debug("将图片文件写入目录完成。");

        nlohmann::json result;
        result["id"]   = image_id;
        result["path"] = rel_path;
        spdlog::info("上传照片墙图片完成。");
        return { std::nullopt, result };
    }

} // namespace img
