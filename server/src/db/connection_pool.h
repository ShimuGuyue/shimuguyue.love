/**
 * @file db/connection_pool.h
 * @brief 数据库连接池的定义
 */
#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <string>
#include <utility>

#include <pqxx/pqxx>
#include <libcpp-pg-pool.hpp>

namespace db
{
/**
 * @brief PostgreSQL 连接池（在 lklibs::PgPool 之上包一层保护）。
 *
 * 持有 DB_POOL_SIZE 条常驻 pqxx::connection，内部由 libcpp-pg-pool 管理空闲/占用，
 * 在此之上补充三项保护：
 * - 取用连接带超时：池满时不会无限阻塞，而是返回错误供上层返回 503；
 * - 取用连接即探活：连接已失效（数据库重启、空闲连接被清理、网络中断）时原地重建，
 *   避免失效连接被反复交给业务；
 * - 连接串带 connect_timeout 与 statement_timeout / lock_timeout，
 *   建连与单条 SQL 都不会无限等待。
 */
class ConnectionPool
{
private:
    /// 底层连接池，负责连接的存放与归还。
    std::unique_ptr<lklibs::PgPool> pool_;

    /// 空闲连接额度，与池中可用连接一一对应，用于实现带超时的取用。
    std::unique_ptr<std::counting_semaphore<>> slots_;

    /// 连接串，重建失效连接时使用。
    std::string conninfo_;

    /// 池大小。
    std::size_t pool_size_ = 0;

    /// 保护 last_rebuild_failure_。
    std::mutex rebuild_mutex_;

    /// 上一次重建失败的时间，用于节流重建，避免数据库不可用时反复建连。
    std::chrono::steady_clock::time_point last_rebuild_failure_{};

    ConnectionPool() = default;
    ConnectionPool(const ConnectionPool&) = delete;
    auto operator=(const ConnectionPool&) -> ConnectionPool& = delete;

    /**
     * @brief 原地重建失效的连接。
     * @param conn 待重建的连接。
     * @return 重建成功返回 true。
     */
    [[nodiscard]] auto rebuild(pqxx::connection& conn) -> bool;

    /**
     * @brief 确认连接可用，失效则重建。
     * @param conn 待确认的连接。
     * @return 连接不可用且重建失败时返回错误信息，否则返回 std::nullopt。
     */
    [[nodiscard]] auto ensure_alive(pqxx::connection& conn) -> std::optional<std::string>;

public:
    /**
     * @brief 获取全局连接池单例。
     * @return 连接池引用。
     */
    [[nodiscard]] static auto instance() -> ConnectionPool&;

public:
    /**
     * @brief 初始化连接池，一次性建立 size 条常驻连接。
     * @param size 池大小。
     */
    void create(std::size_t size);

    /**
     * @brief 获取一条独占数据库连接。
     *
     * 池满时最多等待 ACQUIRE_TIMEOUT，超时或连接不可用且重建失败时返回错误信息。
     * @return 独占连接句柄（引用归零时自动归还连接池）；失败时为错误信息。
     */
    [[nodiscard]] auto acquire() -> std::expected<std::shared_ptr<pqxx::connection>, std::string>;

};

    /**
     * @brief 获取独占连接执行数据库回调；回调结束或异常时连接自动归还。
     *
     * 取用连接失败（池满等待超时或重建失败）时不执行回调，直接返回错误信息。
     * 第三方 libpqxx 抛出的异常不在此处理，由 httplib 在最外层统一兜底。
     * @param fn 数据库回调，参数为可用的独占连接。
     * @return 取用连接失败时返回错误信息，成功时返回 std::nullopt。
     */
    template <typename Fn>
    requires std::invocable<Fn, pqxx::connection&>
    auto with_db(Fn&& fn) -> std::optional<std::string>
    {
        auto conn = ConnectionPool::instance().acquire();
        if (!conn)
            return conn.error();

        std::forward<Fn>(fn)(**conn);
        return std::nullopt;
    }

} // namespace db
