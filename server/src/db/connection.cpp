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
     * @brief 检查项目所需数据库表
     */
    void check(pqxx::connection& conn)
    {
        spdlog::debug("正在检查项目所需数据库表...");

        const std::vector<std::string> required_tables = {
            "users", "permissions", "user_permissions",
            "sessions",
            "blogs", "categories", "tags", "blog_tags",
            "images",
            "profile", "about"
        };

        pqxx::nontransaction txn{ conn };

        // 数据库表存在检查
        for (const auto& table_name : required_tables)
        {
            auto res = txn.exec(
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

        // 数据库表额外检查
        // profile 第一条数据存在
        auto profile_res = txn.exec(
            "SELECT 1 FROM profile LIMIT 1"
        );
        if (profile_res.empty())
        {
            spdlog::error("profile 表第一条数据不存在！");
            std::exit(1);
        }
        else
        {
            spdlog::debug("profile 表第一条数据存在。");
        }
        // about 第一条数据存在
        auto about_res = txn.exec(
            "SELECT 1 FROM about LIMIT 1"
        );
        if (about_res.empty())
        {
            spdlog::error("about 表第一条数据不存在！");
            std::exit(1);
        }
        else
        {
            spdlog::debug("about 表第一条数据存在。");
        }

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
