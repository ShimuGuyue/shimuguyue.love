/**
 * @file friend/friend_queries.h
 * @brief 友情链接数据库查询
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace friends
{
    /**
     * @brief 获取所有友情链接记录。
     *
     * 每项包含 name、url、description，以及根据站点名（图片文件名）在
     * FILE_PATH/image/friend_avatars 下匹配到的 image 路径；未匹配到图片时为空串。
     *
     * @param conn 数据库连接。
     * @return JSON 数组。
     */
    [[nodiscard]] auto get_all_friends(pqxx::connection& conn) -> nlohmann::json;

    /**
     * @brief 更新一个友情链接记录（按 old_name 定位，可同时修改 name/url/description）。
     *
     * 若修改了站点名，会检查新站点名是否与其他记录重复；不存在的原始记录返回错误。
     *
     * @param conn        数据库连接。
     * @param old_name    待更新记录的原始站点名。
     * @param name        新的站点名。
     * @param url         新的站点链接。
     * @param description 新的站点描述。
     * @return std::nullopt 表示成功；否则返回错误消息。
     */
    [[nodiscard]] auto update_friend(
        pqxx::connection& conn,
        std::string_view  old_name,
        std::string_view  name,
        std::string_view  url,
        std::string_view  description)
    -> std::optional<std::string>;

} // namespace friends
