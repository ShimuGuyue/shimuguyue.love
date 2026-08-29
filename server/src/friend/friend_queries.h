/**
 * @file friend/friend_queries.h
 * @brief 友情链接数据库查询
 */
#pragma once

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

} // namespace friends
