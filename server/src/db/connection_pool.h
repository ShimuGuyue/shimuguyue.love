/**
 * @file db/connection_pool.h
 * @brief 数据库连接池的定义
 */
#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include <pqxx/pqxx>
#include <libcpp-pg-pool.hpp>

namespace db
{
/**
 * @brief PostgreSQL 连接池（基于 lklibs::PgPool）。
 *
 * 持有 DB_POOL_SIZE 条常驻 pqxx::connection，内部由 libcpp-pg-pool
 * 管理空闲/占用；无空闲连接时阻塞等待。
 */
class ConnectionPool
{
private:
    std::unique_ptr<lklibs::PgPool> pool_;

    ConnectionPool() = default;
    ConnectionPool(const ConnectionPool&) = delete;
    auto operator=(const ConnectionPool&) -> ConnectionPool& = delete;

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
     * @brief 获取一条独占数据库连接；无空闲时阻塞等待。
     * @return 独占连接句柄（引用归零时自动归还连接池）。
     */
    [[nodiscard]] auto acquire() -> std::shared_ptr<pqxx::connection>;

};

    /**
     * @brief 获取独占连接执行数据库回调；回调结束或异常时连接自动归还。
     *
     * 第三方 libpqxx 抛出的异常不在此处理，由 httplib 在最外层统一兜底。
     */
    template <typename Fn>
    requires std::invocable<Fn, pqxx::connection&>
    void with_db(Fn&& fn)
    {
        auto conn = ConnectionPool::instance().acquire();
        std::forward<Fn>(fn)(*conn);
    }

} // namespace db
