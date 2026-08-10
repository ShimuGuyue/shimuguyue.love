/**
 * @file db/connection.h
 * @brief 数据库连接管理函数的定义
 */
#pragma once

namespace db
{
    /**
     * @brief 初始化数据库连接池并检查项目所需数据库表。
     */
    void init();

} // namespace db
