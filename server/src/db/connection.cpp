/**
 * @file db/connection.cpp
 * @brief 数据库连接管理函数的实现
 */

#include "db/connection.h"

#include <cstdlib>
#include <string>
#include <vector>

#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

#include "config/env.h"
#include "db/connection_pool.h"

namespace
{
    /**
     * @brief 检查项目所需数据库表是否存在。
     * @param conn 数据库连接。
     */
    void check_tables_exist(pqxx::connection& conn)
    {
        const std::vector<std::string> required_tables = {
            "users", "permissions", "user_permissions",
            "sessions",
            "blogs", "categories", "tags", "blog_tags",
            "images",
            "profile", "about"
        };

        pqxx::nontransaction txn{ conn };

        for (const auto& table_name : required_tables)
        {
            const auto res = txn.exec(
                "SELECT 1 FROM information_schema.tables "
                "WHERE table_schema = 'public' AND table_name = $1",
                pqxx::params{table_name}
            );

            if (res.empty())
            {
                spdlog::error("数据库表 {} 缺失！", table_name);
                std::exit(1);
            }
            else
            {
                spdlog::debug("数据库表 {} 存在。", table_name);
            }
        }
    }

    /**
     * @brief 检查单行表的结构性数据：首行必须存在。
     * @param conn 数据库连接。
     */
    void check_table_structures(pqxx::connection& conn)
    {
        // 单行表：表中必须存在初始数据行
        const std::vector<std::string> single_row_tables = {
            "profile", "about"
        };

        pqxx::nontransaction txn{ conn };

        for (const auto& table_name : single_row_tables)
        {
            const auto res = txn.exec(
                "SELECT 1 FROM " + table_name + " LIMIT 1"
            );
            if (res.empty())
            {
                spdlog::error("{} 表第一条数据不存在！", table_name);
                std::exit(1);
            }
            else
            {
                spdlog::debug("{} 表第一条数据存在。", table_name);
            }
        }
    }

    /**
     * @brief 检查项目所需数据库表：表存在性 + 表结构。
     * @param conn 数据库连接。
     */
    void check(pqxx::connection& conn)
    {
        spdlog::debug("正在检查项目所需数据库表...");
        check_tables_exist(conn);
        check_table_structures(conn);

        spdlog::info("数据库表检查完成。");
    }

}





namespace db
{
    void init()
    {
        const auto pool_size = static_cast<std::size_t>(
            std::stoull(config::env()["DB_POOL_SIZE"])
        );
        ConnectionPool::instance().create(pool_size);

        with_db(
            [](pqxx::connection& conn)
            {
                check(conn);
            }
        );
    }

} // namespace db
