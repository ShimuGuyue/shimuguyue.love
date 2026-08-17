/**
 * @file export/export_queries.h
 * @brief 数据导出查询：将各数据表读取为 JSON 数组
 */
#pragma once

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace export_queries
{
    /**
     * @brief 导出 users 表为 JSON 数组。
     * @param txn 当前事务。
     * @return JSON 数组。
     */
    [[nodiscard]] auto query_users(pqxx::work& txn) -> nlohmann::json;

    /**
     * @brief 导出 permissions 表为 JSON 数组。
     * @param txn 当前事务。
     * @return JSON 数组。
     */
    [[nodiscard]] auto query_permissions(pqxx::work& txn) -> nlohmann::json;

    /**
     * @brief 导出 user_permissions 关联表为 JSON 数组。
     * @param txn 当前事务。
     * @return JSON 数组。
     */
    [[nodiscard]] auto query_user_permissions(pqxx::work& txn) -> nlohmann::json;

    /**
     * @brief 导出 blogs 表为 JSON 数组。
     * @param txn 当前事务。
     * @return JSON 数组。
     */
    [[nodiscard]] auto query_blogs(pqxx::work& txn) -> nlohmann::json;

    /**
     * @brief 导出 categories 表为 JSON 数组。
     * @param txn 当前事务。
     * @return JSON 数组。
     */
    [[nodiscard]] auto query_categories(pqxx::work& txn) -> nlohmann::json;

    /**
     * @brief 导出 tags 表为 JSON 数组。
     * @param txn 当前事务。
     * @return JSON 数组。
     */
    [[nodiscard]] auto query_tags(pqxx::work& txn) -> nlohmann::json;

    /**
     * @brief 导出 blog_tags 关联表为 JSON 数组。
     * @param txn 当前事务。
     * @return JSON 数组。
     */
    [[nodiscard]] auto query_blog_tags(pqxx::work& txn) -> nlohmann::json;

} // namespace export_queries
