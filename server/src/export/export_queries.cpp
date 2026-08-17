/**
 * @file export/export_queries.cpp
 * @brief 数据导出查询实现
 */

#include "export/export_queries.h"

#include <nlohmann/json.hpp>

namespace export_queries
{
    auto query_users(pqxx::work& txn) -> nlohmann::json
    {
        const auto rows = txn.exec(
            "SELECT id, enabled, key_hash, key_enabled, username, password_hash "
            "FROM users ORDER BY id"
        );
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            arr.push_back({
                { "id",            row["id"]         .as<int>() },
                { "enabled",       row["enabled"]    .as<bool>() },
                { "key_hash",      row["key_hash"]   .as<std::string>() },
                { "key_enabled",   row["key_enabled"].as<bool>() },
                { "username",      row["username"]     .is_null()
                                 ? nlohmann::json(nullptr)
                                 : nlohmann::json(row["username"]     .as<std::string>()) },
                { "password_hash", row["password_hash"].is_null()
                                 ? nlohmann::json(nullptr)
                                 : nlohmann::json(row["password_hash"].as<std::string>()) }
            });
        }
        return arr;
    }

    auto query_permissions(pqxx::work& txn) -> nlohmann::json
    {
        const auto rows = txn.exec("SELECT id, name FROM permissions ORDER BY id");
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            arr.push_back({
                { "id",   row["id"]  .as<int>() },
                { "name", row["name"].as<std::string>() },
            });
        }
        return arr;
    }

    auto query_user_permissions(pqxx::work& txn) -> nlohmann::json
    {
        const auto rows = txn.exec(
            "SELECT user_id, permission_id FROM user_permissions ORDER BY user_id, permission_id"
        );
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            arr.push_back({
                { "user_id",       row["user_id"]      .as<int>() },
                { "permission_id", row["permission_id"].as<int>() },
            });
        }
        return arr;
    }

    auto query_blogs(pqxx::work& txn) -> nlohmann::json
    {
        const auto rows = txn.exec(
            "SELECT id, title, description, TO_CHAR(update_time, 'YYYY-MM-DD') AS update_time, "
            "content, file_path, category_id FROM blogs ORDER BY id"
        );
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            arr.push_back({
                { "id",          row["id"]         .as<int>() },
                { "title",       row["title"]      .as<std::string>() },
                { "description", row["description"].as<std::string>() },
                { "update_time", row["update_time"].as<std::string>() },
                { "content",     row["content"]    .as<std::string>() },
                { "file_path",   row["file_path"]  .as<std::string>() },
                { "category_id", row["category_id"].is_null()
                               ? nlohmann::json(nullptr)
                               : nlohmann::json(row["category_id"].as<int>()) },
            });
        }
        return arr;
    }

    auto query_categories(pqxx::work& txn) -> nlohmann::json
    {
        const auto rows = txn.exec("SELECT id, name FROM categories ORDER BY id");
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            arr.push_back({
                { "id",   row["id"]  .as<int>() },
                { "name", row["name"].as<std::string>() },
            });
        }
        return arr;
    }

    auto query_tags(pqxx::work& txn) -> nlohmann::json
    {
        const auto rows = txn.exec("SELECT id, name, category_id FROM tags ORDER BY id");
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            arr.push_back({
                { "id",          row["id"]         .as<int>() },
                { "name",        row["name"]       .as<std::string>() },
                { "category_id", row["category_id"].as<int>() },
            });
        }
        return arr;
    }

    auto query_blog_tags(pqxx::work& txn) -> nlohmann::json
    {
        const auto rows = txn.exec(
            "SELECT blog_id, tag_id FROM blog_tags ORDER BY blog_id, tag_id"
        );
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& row : rows)
        {
            arr.push_back({
                { "blog_id", row["blog_id"].as<int>() },
                { "tag_id",  row["tag_id"] .as<int>() },
            });
        }
        return arr;
    }

} // namespace export_queries
