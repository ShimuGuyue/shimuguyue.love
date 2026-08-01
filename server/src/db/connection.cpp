/**
 * @file db/connection.cpp
 * @brief 数据库连接管理函数的实现
 */

#include "db/connection.h"

#include "config/env.h"

#include <cstdlib>
#include <format>
#include <string>
#include <spdlog/spdlog.h>

namespace db
{

static pqxx::connection conn;

void init()
{
    spdlog::info("正在连接至 PostgreSQL...");

    auto host     = config::get_env("PGHOST");
    auto port     = config::get_env("PGPORT");
    auto dbname   = config::get_env("PGDATABASE");
    auto user     = config::get_env("PGUSER");
    auto password = config::get_env("PGPASSWORD");

    auto conn_str = std::format(
        "host={} port={} dbname={} user={} password={}",
        host, port, dbname, user, password
    );
    conn = pqxx::connection{ conn_str };

    if (!conn.is_open())
    {
        spdlog::error("连接至 PostgreSQL 失败！");
        exit(1);
    }
    spdlog::info("成功连接至 PostgreSQL。");
}

const auto connection() -> pqxx::connection&
{
    return conn;
}

} // namespace db
