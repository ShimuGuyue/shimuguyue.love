/**
 * @file friend/friend_queries.cpp
 * @brief 友情链接数据库查询实现
 */

#include "friend/friend_queries.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

    /**
     * @brief 从位图文件头部解析图片尺寸（宽、高），支持 PNG/JPEG/GIF/WebP/BMP。
     *
     * @param data 文件内容。
     * @return 解析成功返回 (宽, 高)；无法识别返回 std::nullopt。
     */
    auto parse_image_dimensions(std::string_view data)
    -> std::optional<std::pair<int, int>>
    {
        if (data.empty())
            return std::nullopt;

        // 检测格式签名
        // PNG：签名 8 字节后接 IHDR，宽/高为 24 位大端，位于偏移 16 / 20。
        if (data.size() >= 24
        &&  static_cast<unsigned char>(data[0]) == 0x89
        &&  data[1] == 'P' && data[2] == 'N' && data[3] == 'G'
        &&  data[4] == '\r' && data[5] == '\n' && data[6] == '\x1a' && data[7] == '\n')
        {
            const auto w = (static_cast<uint32_t>(static_cast<unsigned char>(data[16])) << 24)
                         | (static_cast<uint32_t>(static_cast<unsigned char>(data[17])) << 16)
                         | (static_cast<uint32_t>(static_cast<unsigned char>(data[18])) << 8)
                         | static_cast<uint32_t>(static_cast<unsigned char>(data[19]));
            const auto h = (static_cast<uint32_t>(static_cast<unsigned char>(data[20])) << 24)
                         | (static_cast<uint32_t>(static_cast<unsigned char>(data[21])) << 16)
                         | (static_cast<uint32_t>(static_cast<unsigned char>(data[22])) << 8)
                         | static_cast<uint32_t>(static_cast<unsigned char>(data[23]));
            return std::pair{ static_cast<int>(w), static_cast<int>(h) };
        }
        // GIF：GIF87a/GIF89a，宽/高为 2 字节小端，位于偏移 6 / 8。
        if (data.size() >= 10
        &&  (data.substr(0, 6) == "GIF87a" || data.substr(0, 6) == "GIF89a"))
        {
            const auto w = static_cast<unsigned char>(data[6]) | (static_cast<unsigned char>(data[7]) << 8);
            const auto h = static_cast<unsigned char>(data[8]) | (static_cast<unsigned char>(data[9]) << 8);
            return std::pair{ static_cast<int>(w), static_cast<int>(h) };
        }
        // BMP：'BM'，宽/高为 4 字节小端，位于偏移 18 / 22。
        if (data.size() >= 26 && data[0] == 'B' && data[1] == 'M')
        {
            const auto w = static_cast<unsigned char>(data[18])
                         | (static_cast<unsigned char>(data[19]) << 8)
                         | (static_cast<unsigned char>(data[20]) << 16)
                         | (static_cast<unsigned char>(data[21]) << 24);
            const auto h = static_cast<unsigned char>(data[22])
                         | (static_cast<unsigned char>(data[23]) << 8)
                         | (static_cast<unsigned char>(data[24]) << 16)
                         | (static_cast<unsigned char>(data[25]) << 24);
            return std::pair{ static_cast<int>(w), static_cast<int>(h) };
        }
        // WebP：RIFF....WEBP + 子块。
        if (data.size() >= 30 && data.substr(0, 4) == "RIFF" && data.substr(8, 4) == "WEBP")
        {
            if (data.substr(12, 4) == "VP8 ")
            {
                const auto w = (static_cast<unsigned char>(data[26]) | (static_cast<unsigned char>(data[27]) << 8)) & 0x3FFF;
                const auto h = (static_cast<unsigned char>(data[28]) | (static_cast<unsigned char>(data[29]) << 8)) & 0x3FFF;
                return std::pair{ static_cast<int>(w), static_cast<int>(h) };
            }
            if (data.substr(12, 4) == "VP8L")
            {
                const auto b0 = static_cast<unsigned char>(data[21]);
                const auto b1 = static_cast<unsigned char>(data[22]);
                const auto b2 = static_cast<unsigned char>(data[23]);
                const auto b3 = static_cast<unsigned char>(data[24]);
                const auto w = static_cast<uint32_t>(b0 | ((b1 & 0x3F) << 8)) + 1;
                const auto h = static_cast<uint32_t>((b1 >> 6) | (b2 << 2) | ((b3 & 0x0F) << 10)) + 1;
                return std::pair{ static_cast<int>(w), static_cast<int>(h) };
            }
            if (data.substr(12, 4) == "VP8X")
            {
                const auto w = static_cast<unsigned char>(data[21])
                             | (static_cast<unsigned char>(data[22]) << 8)
                             | (static_cast<unsigned char>(data[23]) << 16);
                const auto h = static_cast<unsigned char>(data[24])
                             | (static_cast<unsigned char>(data[25]) << 8)
                             | (static_cast<unsigned char>(data[26]) << 16);
                return std::pair{ static_cast<int>(w) + 1, static_cast<int>(h) + 1 };
            }
            return std::nullopt;
        }
        // JPEG：FFD8 起始，扫描 SOF 段获取高度/宽度。
        if (data.size() >= 4
        &&  static_cast<unsigned char>(data[0]) == 0xFF
        &&  static_cast<unsigned char>(data[1]) == 0xD8)
        {
            std::size_t pos = 2;
            while (pos + 4 <= data.size())
            {
                if (static_cast<unsigned char>(data[pos]) != 0xFF)
                {
                    ++pos;
                    continue;
                }
                const auto marker = static_cast<unsigned char>(data[pos + 1]);
                if (marker == 0xFF)
                {
                    pos += 1;
                    continue;
                }
                if (marker == 0x00 || marker == 0xD8 || marker == 0x01
                || (marker >= 0xD0 && marker <= 0xD7))
                {
                    pos += 2;
                    continue;
                }
                if (marker == 0xD9)
                    break;
                if (pos + 4 > data.size())
                    break;
                const auto seg_len = (static_cast<unsigned char>(data[pos + 2]) << 8)
                                   | static_cast<unsigned char>(data[pos + 3]);
                const bool is_sof = marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
                if (is_sof && pos + 9 <= data.size())
                {
                    const auto h = (static_cast<unsigned char>(data[pos + 5]) << 8)
                                 | static_cast<unsigned char>(data[pos + 6]);
                    const auto w = (static_cast<unsigned char>(data[pos + 7]) << 8)
                                 | static_cast<unsigned char>(data[pos + 8]);
                    return std::pair{ static_cast<int>(w), static_cast<int>(h) };
                }
                pos += 2 + seg_len;
            }
            return std::nullopt;
        }

        return std::nullopt;
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

        // 扫描 friend_avatars 目录，建立“文件名 stem → 带版本号的访问 URL”映射，
        // 便于按站点名（图片名等于站点名）匹配实际图片；带 ?v= 修改时间可避免浏览器缓存旧图。
        std::unordered_map<std::string, std::string> avatar_url_by_stem;
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
                const auto mtime = std::chrono::duration_cast<std::chrono::milliseconds>(
                    entry.last_write_time(ec).time_since_epoch()
                ).count();
                avatar_url_by_stem[entry.path().stem().string()] =
                    "/image/friend_avatars/" + filename + "?v=" + std::to_string(mtime);
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

            if (const auto it = avatar_url_by_stem.find(name); it != avatar_url_by_stem.end())
                item["image"] = it->second;
            else
                item["image"] = "";

            arr.push_back(std::move(item));
        }

        spdlog::debug("获取友情链接完成。");
        return arr;
    }

    auto create_friend(
        pqxx::connection& conn,
        std::string_view  name,
        std::string_view  url,
        std::string_view  description)
    -> std::optional<std::string>
    {
        spdlog::debug("正在创建友情链接 ...");
        pqxx::work txn{ conn };

        // 站点名唯一，先检查是否与其他记录重复。
        const auto dup = txn.exec(
            "SELECT id FROM friends WHERE name = $1",
            pqxx::params{ std::string{ name } }
        );
        if (!dup.empty())
        {
            txn.commit();
            spdlog::warn("创建友情链接失败：站点名 {} 已存在。", std::string{ name });
            return "站点名已存在";
        }

        txn.exec(
            "INSERT INTO friends (name, url, description) VALUES ($1, $2, $3)",
            pqxx::params{
                std::string{ name },
                std::string{ url },
                std::string{ description }
            }
        );
        txn.commit();
        spdlog::info("创建友情链接成功：{}.", std::string{ name });
        return std::nullopt;
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

    auto upload_avatar(
        pqxx::connection& conn,
        std::string_view  name,
        std::string_view  filename,
        std::string_view  data)
    -> std::pair<std::optional<std::string>, nlohmann::json>
    {
        spdlog::debug("正在上传友链头像 ...");

        // 校验扩展名。
        const auto ext_pos = filename.rfind('.');
        if (ext_pos == std::string_view::npos)
            return { std::string{ "文件缺少扩展名" }, {} };
        const auto ext = filename.substr(ext_pos);
        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".webp")
            return { std::string{ "不支持的文件格式" }, {} };
        if (name.empty())
            return { std::string{ "站点名不能为空" }, {} };

        // 校验图片尺寸必须为 512×512。
        const auto dims = parse_image_dimensions(data);
        if (!dims)
            return { std::string{ "无法识别图片尺寸" }, {} };
        if (dims->first != 512 || dims->second != 512)
            return { std::string{ "图片尺寸必须为 512×512" }, {} };

        // 校验站点存在。
        {
            pqxx::work txn{ conn };
            const auto rows = txn.exec(
                "SELECT id FROM friends WHERE name = $1",
                pqxx::params{ std::string{ name } }
            );
            if (rows.empty())
                return { std::string{ "站点不存在" }, {} };
        }

        // 确保目录存在。
        const auto avatars_dir = std::filesystem::path{ config::env()["FILE_PATH"] } / "image" / "friend_avatars";
        std::error_code ec;

        // 删除同站点名的旧头像（任意扩展名），实现替换。
        for (const auto& entry : std::filesystem::directory_iterator(avatars_dir, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file(ec))
                continue;
            if (ec)
                break;
            if (entry.path().stem().string() == name)
            {
                std::error_code rm_ec;
                std::filesystem::remove(entry.path(), rm_ec);
            }
        }

        // 写入新头像文件。
        const auto new_filename = std::string{ name } + std::string{ ext };
        const auto full = (avatars_dir / new_filename).string();
        std::ofstream ofs{ full, std::ios::binary };
        if (!ofs)
            return { std::string{ "写入文件失败" }, {} };
        ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!ofs)
            return { std::string{ "写入文件失败" }, {} };
        ofs.close();

        spdlog::info("友链头像上传成功：{}.", new_filename);
        std::error_code mtime_ec;
        const auto mtime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::filesystem::last_write_time(avatars_dir / new_filename, mtime_ec).time_since_epoch()
        ).count();
        nlohmann::json result;
        result["image"] = "/image/friend_avatars/" + new_filename + "?v=" + std::to_string(mtime);
        return { std::nullopt, result };
    }

} // namespace friends
