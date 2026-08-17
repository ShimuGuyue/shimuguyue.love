/**
 * @file export/export_data.cpp
 * @brief 后台数据导出实现：查询数据表并打包为 zip
 */

#include "export/export_data.h"

#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "export/export_queries.h"
#include "export/zip_writer.h"

namespace export_data
{
    auto build_blogs_export_zip(pqxx::connection& conn) -> std::expected<std::string, std::string>
    {
        pqxx::work txn{ conn };
        const auto      blogs_json = export_queries::query_blogs     (txn);
        const auto categories_json = export_queries::query_categories(txn);
        const auto       tags_json = export_queries::query_tags      (txn);
        const auto  blog_tags_json = export_queries::query_blog_tags (txn);
        txn.commit();

        const std::vector<std::pair<std::string, std::string>> files = {
            {      "blogs.json",      blogs_json.dump(2) },
            { "categories.json", categories_json.dump(2) },
            {       "tags.json",       tags_json.dump(2) },
            {  "blog_tags.json",  blog_tags_json.dump(2) }
        };
        spdlog::info("博客数据导出完成。");
        return zip_writer::build_zip(files);
    }

    auto build_users_export_zip(pqxx::connection& conn) -> std::expected<std::string, std::string>
    {
        pqxx::work txn{ conn };
        const auto            users_json = export_queries::query_users           (txn);
        const auto      permissions_json = export_queries::query_permissions     (txn);
        const auto user_permissions_json = export_queries::query_user_permissions(txn);
        txn.commit();

        const std::vector<std::pair<std::string, std::string>> files = {
            {            "users.json",            users_json.dump(2) },
            {      "permissions.json",      permissions_json.dump(2) },
            { "user_permissions.json", user_permissions_json.dump(2) },
        };
        spdlog::info("用户数据导出完成。", files.size());
        return zip_writer::build_zip(files);
    }

} // namespace export_data
