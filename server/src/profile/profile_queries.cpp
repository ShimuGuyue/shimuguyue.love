/**
 * @file profile/profile_queries.cpp
 * @brief 个人介绍数据库查询实现
 */

#include "profile/profile_queries.h"

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

namespace profile
{

auto get_profile(pqxx::connection& conn) -> nlohmann::json
{
    spdlog::info("正在从数据库获取个人简介...");
    pqxx::work txn{ conn };
    const auto rows = txn.exec(
        "SELECT title, subtitle, bio FROM profile WHERE id = 1"
    );
    const auto& row = rows[0];

    nlohmann::json json;
    json["title"]    = row["title"]   .as<std::string>();
    json["subtitle"] = row["subtitle"].as<std::string>();
    json["bio"]      = row["bio"]     .as<std::string>();
    txn.commit();
    spdlog::info("获取个人简介成功。");
    return json;
}

auto update_profile(
    pqxx::connection& conn,
    std::string_view  title,
    std::string_view  subtitle,
    std::string_view  bio)
-> std::string
{
    spdlog::info("正在向数据库更新个人简介...");
    pqxx::work txn{ conn };
    txn.exec(
        "UPDATE profile SET title = $1, subtitle = $2, bio = $3 WHERE id = 1",
        pqxx::params{
            std::string{ title },
            std::string{ subtitle },
            std::string{ bio }
        }
    );
    txn.commit();
    spdlog::info("更新个人简介成功。");
    return {};
}

} // namespace profile
