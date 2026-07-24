/**
 * @file profile/profile_queries.cpp
 * @brief 个人介绍数据库查询实现
 */

#include "profile/profile_queries.h"

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace profile {

auto get_profile(pqxx::connection& conn) -> nlohmann::json
{
    pqxx::work txn{ conn };
    const auto rows = txn.exec(
        "SELECT title, subtitle, bio FROM profile WHERE id = 1"
    );

    nlohmann::json j;
    if (!rows.empty())
    {
        const auto& row = rows[0];
        j["title"]    = row["title"].as<std::string>();
        j["subtitle"] = row["subtitle"].as<std::string>();
        j["bio"]      = row["bio"].as<std::string>();
    }
    txn.commit();
    return j;
}

auto update_profile(
    pqxx::connection& conn,
    std::string_view  title,
    std::string_view  subtitle,
    std::string_view  bio)
-> std::string
{
    pqxx::work txn{ conn };
    txn.exec(
        "UPDATE profile SET title = $1, subtitle = $2, bio = $3 WHERE id = 1",
        pqxx::params{
            std::string{title},
            std::string{subtitle},
            std::string{bio}
        }
    );
    txn.commit();
    return {};
}

} // namespace profile
