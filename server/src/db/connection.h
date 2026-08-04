/**
 * @file db/connection.h
 * @brief 数据库连接管理函数的定义
 */
#pragma once

#include <pqxx/pqxx>

namespace db
{
    /**
     * @brief 初始化数据库连接。
     */
    void init();

    /**
     * @brief 获取数据库连接的 pqxx::connection
     * @return 已打开的 pqxx::connection。
     */
    [[nodiscard]] auto connection() -> pqxx::connection&;

} // namespace db
