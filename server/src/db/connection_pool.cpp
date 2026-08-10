/**
 * @file db/connection_pool.cpp
 * @brief 数据库连接池的实现
 */

#include "db/connection_pool.h"

#include <cstdlib>
#include <format>
#include <string>

#include <libpq-fe.h>
#include <spdlog/spdlog.h>

#include "config/env.h"

namespace db
{
    auto ConnectionPool::instance() -> ConnectionPool&
    {
        static ConnectionPool pool;
        return pool;
    }

    void ConnectionPool::create(std::size_t size)
    {
        const auto& env      = config::env();
        const auto  conninfo = std::format(
            "host={} port={} dbname={} user={} password={} "
            "keepalives=1 keepalives_idle=60 keepalives_interval=10 keepalives_count=5",
            env["PGHOST"], env["PGPORT"], env["PGDATABASE"], env["PGUSER"], env["PGPASSWORD"]
        );

        // 启动时探测一次数据库连通性，连通失败终止程序。
        auto* raw = PQconnectdb(conninfo.c_str());
        if (raw == nullptr || PQstatus(raw) != CONNECTION_OK)
        {
            spdlog::error(
                "连接至 PostgreSQL 失败：{}",
                raw == nullptr ? "内存不足" : PQerrorMessage(raw)
            );
            if (raw != nullptr)
            {
                PQfinish(raw);
            }
            std::exit(1);
        }
        PQfinish(raw);

        pool_ = std::make_unique<lklibs::PgPool>(conninfo, size);
        spdlog::info("数据库连接池创建完成：共 {} 条连接。", size);
    }

    auto ConnectionPool::acquire() -> std::shared_ptr<pqxx::connection>
    {
        return pool_->acquire();
    }

} // namespace db
