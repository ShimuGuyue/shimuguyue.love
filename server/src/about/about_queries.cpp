/**
 * @file about/about_queries.cpp
 * @brief 《关于我》页面数据库查询实现
 */

#include "about/about_queries.h"

#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

namespace about
{
    auto get_about(pqxx::connection& conn) -> std::string
    {
        spdlog::debug("正在获取 README 内容...");
        pqxx::work txn{ conn };
        const auto rows = txn.exec(
            "SELECT content FROM about WHERE id = 1"
        );

        std::string content;
        const auto& row = rows[0];
        if (!row["content"].is_null())
            content = row["content"].as<std::string>();
        txn.commit();
        spdlog::debug("README 内容获取完成。");
        return content;
    }

} // namespace about
