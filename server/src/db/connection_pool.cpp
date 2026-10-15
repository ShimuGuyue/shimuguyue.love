/**
 * @file db/connection_pool.cpp
 * @brief 数据库连接池的实现
 */

#include "db/connection_pool.h"

#include <chrono>
#include <cstdlib>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <libpq-fe.h>
#include <spdlog/spdlog.h>

#include "config/config.h"

namespace
{
    /// 取用连接的最大等待时长。
    constexpr std::chrono::milliseconds ACQUIRE_TIMEOUT{ 5000 };

    /// 建立连接的超时时间（秒）。
    constexpr int CONNECT_TIMEOUT_SECONDS = 5;

    /// 已发出但未被确认的数据允许滞留的最长时间（毫秒），用于识别半开连接。
    constexpr int TCP_USER_TIMEOUT_MS = 15000;

    /// 单条 SQL 在服务端的执行上限（毫秒）。
    constexpr int STATEMENT_TIMEOUT_MS = 15000;

    /// 单条 SQL 等待锁的上限（毫秒）。
    constexpr int LOCK_TIMEOUT_MS = 5000;

    /// 重建失败后的节流时长：该时长内不再尝试重建，避免数据库不可用时反复建连。
    constexpr std::chrono::seconds REBUILD_RETRY_INTERVAL{ 5 };

    /**
     * @brief 拼接数据库连接串。
     *
     * 除基本连接参数外固定带上：建连超时、TCP 用户超时、TCP keepalive 与
     * 服务端的语句 / 锁超时，避免建连或单条 SQL 无限等待。
     * @return 连接串。
     */
    auto build_conninfo() -> std::string
    {
        const auto& config_map = config::config();
        return std::format(
            "host={} port={} dbname={} user={} password={} "
            "connect_timeout={} tcp_user_timeout={} "
            "keepalives=1 keepalives_idle=60 keepalives_interval=10 keepalives_count=5 "
            "options='-c statement_timeout={} -c lock_timeout={}'",
            config_map["PGHOST"],     config_map["PGPORT"], config_map["PGDATABASE"],
            config_map["PGUSER"],     config_map["PGPASSWORD"],
            CONNECT_TIMEOUT_SECONDS,  TCP_USER_TIMEOUT_MS,
            STATEMENT_TIMEOUT_MS,     LOCK_TIMEOUT_MS
        );
    }

    /**
     * @brief 探活连接：发起一次轻量往返，确认连接仍然可用。
     *
     * 连接因数据库重启、空闲连接被清理或网络中继中断而失效时，libpq 的 PQstatus
     * 仍可能报告 CONNECTION_OK，只有真正发起一次往返才能发现，故用 SELECT 1 探活。
     * @param conn 待探活的连接。
     * @return 连接可用返回 true。
     */
    auto probe(pqxx::connection& conn) -> bool
    {
        try
        {
            pqxx::nontransaction txn{ conn };
            const auto rows = txn.exec("SELECT 1");
            return !rows.empty();
        }
        catch (const std::exception& e)
        {
            spdlog::warn("数据库连接探活失败：{}", e.what());
            return false;
        }
    }

} // namespace





namespace db
{
    auto ConnectionPool::instance() -> ConnectionPool&
    {
        static ConnectionPool pool;
        return pool;
    }

    void ConnectionPool::create(std::size_t size)
    {
        conninfo_  = build_conninfo();
        pool_size_ = size;

        // 启动时探测一次数据库连通性，连通失败终止程序。
        auto* raw = PQconnectdb(conninfo_.c_str());
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

        // 建池过程会逐条发起连接，数据库不可用或连接数已满时抛出异常。
        try
        {
            pool_ = std::make_unique<lklibs::PgPool>(conninfo_, size);
        }
        catch (const std::exception& e)
        {
            spdlog::error("数据库连接池创建失败：{}", e.what());
            std::exit(1);
        }

        slots_ = std::make_unique<std::counting_semaphore<>>(size);
        spdlog::info("数据库连接池创建完成：共 {} 条连接。", size);
    }

    auto ConnectionPool::acquire() -> std::expected<std::shared_ptr<pqxx::connection>, std::string>
    {
        // 先取空闲额度：池满时带超时等待，而不是无限阻塞
        const auto start = std::chrono::steady_clock::now();
        if (!slots_->try_acquire_for(ACQUIRE_TIMEOUT))
        {
            const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            spdlog::warn("等待空闲数据库连接超时：已等待 {} 毫秒（池大小 {}）。", waited, pool_size_);
            return std::unexpected(std::format("等待空闲数据库连接超时（已等待 {} 毫秒）", waited));
        }

        // 已持有空闲额度，此处取连接必然立即返回
        auto conn = pool_->acquire();

        if (const auto err = ensure_alive(*conn))
        {
            slots_->release(); // 归还未使用的额度，连接随 conn 析构归还底层连接池
            return std::unexpected(*err);
        }

        // 先取出裸指针：lambda 的捕获初始化会移走 conn，两者求值顺序不定
        auto* const raw = conn.get();

        // 连接归零时先归还底层连接池，再归还额度
        return std::shared_ptr<pqxx::connection>(
            raw,
            [this, conn = std::move(conn)](pqxx::connection*) mutable
            {
                conn.reset();
                slots_->release();
            }
        );
    }

    auto ConnectionPool::rebuild(pqxx::connection& conn) -> bool
    {
        spdlog::warn("检测到失效的数据库连接，正在重建...");
        try
        {
            conn = pqxx::connection{ conninfo_ };
            return true;
        }
        catch (const std::exception& e)
        {
            spdlog::error("数据库连接重建失败：{}", e.what());
            return false;
        }
    }

    auto ConnectionPool::ensure_alive(pqxx::connection& conn) -> std::optional<std::string>
    {
        if (probe(conn))
        {
            return std::nullopt;
        }

        // 数据库整体不可用时节流重建，避免每个请求都去建连
        {
            std::lock_guard lock{ rebuild_mutex_ };
            if (std::chrono::steady_clock::now() - last_rebuild_failure_ < REBUILD_RETRY_INTERVAL)
            {
                return std::string{ "数据库连接不可用（重建失败冷却中）" };
            }
        }

        if (!rebuild(conn))
        {
            std::lock_guard lock{ rebuild_mutex_ };
            last_rebuild_failure_ = std::chrono::steady_clock::now();
            return std::string{ "数据库连接不可用" };
        }

        {
            std::lock_guard lock{ rebuild_mutex_ };
            last_rebuild_failure_ = {};
        }
        spdlog::info("数据库连接重建完成。");
        return std::nullopt;
    }

} // namespace db
