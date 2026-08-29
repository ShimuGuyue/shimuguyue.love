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

namespace
{
    /**
     * @brief 站点名被修改后，将 friend_avatars 下以旧站点名为文件名的头像同步重命名为新站点名。
     *
     * 头像文件名约定为「站点名 + 扩展名」。若存在旧头像文件且目标文件不存在，则重命名；
     * 找不到旧头像或目标文件已存在时跳过，避免覆盖其它头像。重命名失败仅记录告警，不阻断文本编辑。
     *
     * @param old_name 旧站点名。
     * @param name     新站点名。
     */
    void rename_friend_avatar(std::string_view old_name, std::string_view name)
    {
        if (old_name == name)
            return;

        const auto avatars_dir = std::filesystem::path{ config::env()["FILE_PATH"] } / "image" / "friend_avatars";
        std::error_code ec;
        if (!std::filesystem::exists(avatars_dir, ec) || ec)
            return;

        // 找到以旧站点名为文件名的头像文件。
        std::optional<std::filesystem::path> old_avatar;
        for (const auto& entry : std::filesystem::directory_iterator(avatars_dir, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file(ec))
                continue;
            if (ec)
                break;
            if (entry.path().stem().string() == old_name)
            {
                old_avatar = entry.path();
                break;
            }
        }
        if (!old_avatar)
            return; // 无旧头像，无需重命名。

        const auto filename = old_avatar->filename().string();
        const auto new_name = std::string{ name } + old_avatar->extension().string();

        // 目标文件已存在则跳过，避免覆盖其它头像。
        const auto target = avatars_dir / new_name;
        if (std::filesystem::exists(target, ec) && !ec)
        {
            spdlog::warn("更新友链头像：目标文件 {} 已存在，跳过重命名。", new_name);
            return;
        }

        std::filesystem::rename(*old_avatar, target, ec);
        if (ec)
        {
            spdlog::warn("更新友链头像：重命名 {} 失败：{}。", filename, ec.message());
            return;
        }
        spdlog::info("更新友链头像：{} -> {}.", filename, new_name);
    }
}





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

    auto update_friend(
        pqxx::connection& conn,
        std::string_view  old_name,
        std::string_view  name,
        std::string_view  url,
        std::string_view  description)
    -> std::optional<std::string>
    {
        spdlog::debug("正在更新友情链接 ...");
        pqxx::work txn{ conn };

        // 定位原始记录，确保存在。
        const auto rows = txn.exec(
            "SELECT id FROM friends WHERE name = $1",
            pqxx::params{ std::string{ old_name } }
        );
        if (rows.empty())
        {
            txn.commit();
            spdlog::warn("更新友情链接失败：站点 {} 不存在。", std::string{ old_name });
            return "站点不存在";
        }

        // 若修改了站点名，检查新站点名是否与其他记录重复。
        if (old_name != name)
        {
            const auto dup = txn.exec(
                "SELECT id FROM friends WHERE name = $1",
                pqxx::params{ std::string{ name } }
            );
            if (!dup.empty())
            {
                txn.commit();
                spdlog::warn("更新友情链接失败：站点名 {} 已存在。", std::string{ name });
                return "站点名已存在";
            }
        }

        txn.exec(
            "UPDATE friends SET name = $1, url = $2, description = $3 WHERE name = $4",
            pqxx::params{
                std::string{ name },
                std::string{ url },
                std::string{ description },
                std::string{ old_name }
            }
        );
        txn.commit();
        // 站点名被修改时，同步重命名 friend_avatars 目录下对应的头像文件。
        rename_friend_avatar(old_name, name);
        spdlog::info("更新友情链接成功：{} -> {}.", std::string{ old_name }, std::string{ name });
        return std::nullopt;
    }

} // namespace friends
