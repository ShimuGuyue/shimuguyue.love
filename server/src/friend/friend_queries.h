/**
 * @file friend/friend_queries.h
 * @brief 友情链接数据库查询
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace friends
{
    /**
     * @brief 获取所有友情链接记录。
     *
     * 每项包含 name、url、description，以及根据友链 id（图片文件名）在
     * FILE_PATH/image/friend_avatars 下匹配到的 image 路径；未匹配到图片时为空串。
     *
     * @param conn 数据库连接。
     * @return JSON 数组。
     */
    [[nodiscard]] auto get_all_friends(pqxx::connection& conn) -> nlohmann::json;

    /**
     * @brief 新建一个友情链接记录。
     *
     * 站点链接不允许与其他记录重复（友链条目以站点链接区分）；成功时返回 std::nullopt。
     *
     * @param conn        数据库连接。
     * @param name        站点名。
     * @param url         站点链接。
     * @param description 站点描述。
     * @return std::nullopt 表示成功；否则返回错误消息。
     */
    [[nodiscard]] auto create_friend(
        pqxx::connection& conn,
        std::string_view  name,
        std::string_view  url,
        std::string_view  description)
    -> std::optional<std::string>;

    /**
     * @brief 更新一个友情链接记录（按 old_url 定位，可同时修改 name/url/description）。
     *
     * 若修改了站点链接，会检查新站点链接是否与其他记录重复；不存在的原始记录返回错误。
     * 头像文件以友链 id 命名，站点名/站点链接变更均无需重命名头像。
     *
     * @param conn        数据库连接。
     * @param old_url     待更新记录的原始站点链接。
     * @param name        新的站点名。
     * @param url         新的站点链接。
     * @param description 新的站点描述。
     * @return std::nullopt 表示成功；否则返回错误消息。
     */
    [[nodiscard]] auto update_friend(
        pqxx::connection& conn,
        std::string_view  old_url,
        std::string_view  name,
        std::string_view  url,
        std::string_view  description)
    -> std::optional<std::string>;

    /**
     * @brief 上传（替换）一个友情链接的头像文件。
     *
     * 图片仅支持 PNG/JPEG/WebP，且宽高比例必须为 1:1（正方形）。文件名为「友链 id + 扩展名」，
     * 写入会先删除该友链的旧头像（任意扩展名）再写入新文件，实现替换。
     *
     * @param conn     数据库连接。
     * @param url      站点链接（用于定位友链记录，取其 id 作为头像文件名主体）。
     * @param filename 上传文件的原始文件名（用于确定扩展名）。
     * @param data     文件内容。
     * @return 错误信息（std::nullopt 表示成功）与成功时的 JSON（含 image 路径）。
     */
    [[nodiscard]] auto upload_avatar(
        pqxx::connection& conn,
        std::string_view  url,
        std::string_view  filename,
        std::string_view  data)
    -> std::pair<std::optional<std::string>, nlohmann::json>;

} // namespace friends
