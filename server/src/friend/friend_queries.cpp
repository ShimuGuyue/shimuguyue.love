/**
 * @file friend/friend_queries.cpp
 * @brief 友情链接数据库查询实现
 */

#include "friend/friend_queries.h"

#include <filesystem>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>

#include "config/env.h"

namespace friends
{
    auto get_all_friends(pqxx::connection& conn) -> nlohmann::json
    {
        spdlog::debug("正在获取友情链接...");
        pqxx::work txn{ conn };
        const auto rows = txn.exec(
            "SELECT id, name, url, description "
            "FROM friends "
            "ORDER BY id"
        );
        txn.commit();

        // 扫描 friend_avatars 目录，建立“文件名 stem → 完整文件名”映射，
        // 便于按站点名（图片名等于站点名）匹配实际图片。
        std::unordered_map<std::string, std::string> avatar_by_stem;
        const auto avatars_dir = std::filesystem::path{ config::env()["FILE_PATH"] } / "image" / "friend_avatars";
        std::error_code ec;
        if (std::filesystem::exists(avatars_dir, ec) && !ec)
        {
            for (const auto& entry : std::filesystem::directory_iterator(avatars_dir, ec))
            {
                if (ec)
                    break;
                if (!entry.is_regular_file(ec))
                    continue;
                if (ec)
                    break;
                const auto filename = entry.path().filename().string();
                avatar_by_stem[entry.path().stem().string()] = filename;
            }
        }

        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            const auto name = row["name"].as<std::string>();
            nlohmann::json item;
            item["name"]        = name;
            item["url"]         = row["url"].as<std::string>();
            item["description"] = row["description"].as<std::string>();

            if (const auto it = avatar_by_stem.find(name); it != avatar_by_stem.end())
                item["image"] = "/image/friend_avatars/" + it->second;
            else
                item["image"] = "";

            arr.push_back(std::move(item));
        }

        spdlog::debug("获取友情链接完成。");
        return arr;
    }

} // namespace friends
