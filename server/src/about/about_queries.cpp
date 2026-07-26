/**
 * @file about/about_queries.cpp
 * @brief 《关于我》页面数据库查询实现
 */

#include "about/about_queries.h"

#include <pqxx/pqxx>

namespace about {

auto get_about(pqxx::connection& conn) -> std::string
{
    pqxx::work txn{ conn };
    const auto rows = txn.exec(
        "SELECT content FROM about WHERE id = 1"
    );

    std::string content;
    if (!rows.empty())
    {
        const auto& row = rows[0];
        if (!row["content"].is_null())
            content = row["content"].as<std::string>();
    }
    txn.commit();
    return content;
}

} // namespace about
