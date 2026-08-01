/**
 * @file image/image_queries.cpp
 * @brief 照片墙图片数据库查询实现
 */

#include "image/image_queries.h"

#include "config/env.h"

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace img {

static std::string IMAGE_PATH;

void init()
{
    IMAGE_PATH = config::get_env("IMAGE_PATH");

    // 确保子目录存在
    std::error_code ec;
    std::filesystem::create_directories(std::format("{}/home", IMAGE_PATH), ec);
}

auto image_path() -> const std::string&
{
    return IMAGE_PATH;
}

auto get_all_images(pqxx::connection& conn) -> nlohmann::json
{
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
        item["id"]          = row["id"].as<int>();
        item["path"]        = row["path"].as<std::string>();
        item["description"] = row["description"].as<std::string>();
        item["scale"]       = row["scale"].as<double>();
        item["rotation"]    = row["rotation"].as<double>();
        item["pos_x"]       = row["pos_x"].as<double>();
        item["pos_y"]       = row["pos_y"].as<double>();
        item["z"]           = row["z"].as<int>();
        arr.push_back(std::move(item));
    }
    return arr;
}

auto save_image(
    pqxx::connection&  conn,
    std::string_view   path,
    std::string_view   description,
    double             scale,
    double             rotation,
    double             pos_x,
    double             pos_y,
    int                z)
-> std::string
{
    pqxx::work txn{ conn };

    // UPSERT: 存在则更新元数据，不存在则插入
    txn.exec(
        "INSERT INTO images (path, description, scale, rotation, pos_x, pos_y, z) "
        "VALUES ($1, $2, $3, $4, $5, $6, $7) "
        "ON CONFLICT (path) DO UPDATE SET "
        "description = EXCLUDED.description, "
        "scale       = EXCLUDED.scale, "
        "rotation    = EXCLUDED.rotation, "
        "pos_x       = EXCLUDED.pos_x, "
        "pos_y       = EXCLUDED.pos_y, "
        "z           = EXCLUDED.z",
        pqxx::params{
            std::string{path},
            std::string{description},
            scale,
            rotation,
            pos_x,
            pos_y,
            z
        }
    );

    txn.commit();
    return {};
}

auto delete_image(
    pqxx::connection& conn,
    std::string_view  path)
-> std::string
{
    pqxx::work txn{ conn };

    const auto r = txn.exec(
        "DELETE FROM images WHERE path = $1", pqxx::params{ std::string{path} }
    );
    txn.commit();

    if (r.affected_rows() == 0)
        return "图片记录不存在";

    std::error_code ec;
    std::filesystem::path file_path{
        std::format("{}/{}", IMAGE_PATH, path)
    };
    std::filesystem::remove(file_path, ec);

    return {};
}

auto upload_image(
    pqxx::connection& conn,
    std::string_view  filename,
    std::string_view  data)
-> std::pair<std::string, nlohmann::json>
{
    // 校验扩展名
    const auto ext_pos = filename.rfind('.');
    if (ext_pos == std::string::npos)
        return { "文件缺少扩展名", {} };

    const auto ext = filename.substr(ext_pos);
    if (ext != ".jpg" && ext != ".jpeg" && ext != ".png"
        && ext != ".gif" && ext != ".webp" && ext != ".svg")
        return { "不支持的文件格式", {} };

    // 插入数据库获取 id
    std::string rel_path;
    int image_id = 0;
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

    // 写入文件
    const auto full = std::format("{}/{}", IMAGE_PATH, rel_path);
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(full).parent_path(), ec);
    std::ofstream ofs{full, std::ios::binary};
    if (!ofs)
        return { "写入文件失败", {} };
    ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
    ofs.close();

    nlohmann::json result;
    result["id"]   = image_id;
    result["path"] = rel_path;
    return { {}, result };
}

} // namespace img
