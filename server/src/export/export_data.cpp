/**
 * @file export/export_data.cpp
 * @brief 后台数据导出实现：查询数据表并打包为 zip
 */

#include "export/export_data.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "config/env.h"
#include "export/export_queries.h"
#include "export/zip_writer.h"

namespace
{
    /**
     * @brief 递归收集博客目录（FILE_PATH/doc/blogs）下的全部文件。
     * @return 成功返回“zip 内相对路径 / 文件内容”列表；失败返回错误消息。
     */
    auto collect_blog_files() -> std::expected<std::vector<std::pair<std::string, std::string>>, std::string>
    {
        const auto blogs_root = std::filesystem::path{ config::env()["FILE_PATH"] } / "doc" / "blogs";
        std::error_code ec;
        if (!std::filesystem::exists(blogs_root, ec) || ec)
            return std::unexpected{ "博客目录不可用：" + blogs_root.string() };

        std::vector<std::pair<std::string, std::string>> files;
        for (std::filesystem::recursive_directory_iterator it{ blogs_root, ec }, end; !ec && it != end; it.increment(ec))
        {
            if (ec)
                return std::unexpected{ "遍历博客目录失败：" + it->path().string() };

            if (!it->is_regular_file(ec))
                continue;
            if (ec)
                return std::unexpected{ "读取博客目录条目失败：" + it->path().string() };

            const auto rel = std::filesystem::relative(it->path(), blogs_root, ec);
            if (ec)
                return std::unexpected{ "计算博客相对路径失败：" + it->path().string() };

            std::ifstream ifs{ it->path(), std::ios::binary };
            if (!ifs)
                return std::unexpected{ "读取博客文件失败：" + it->path().string() };

            std::ostringstream oss;
            oss << ifs.rdbuf();
            files.emplace_back("blogs/" + rel.generic_string(), oss.str());
        }
        return files;
    }

} // namespace





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

        auto blog_files = collect_blog_files();
        if (!blog_files)
        {
            spdlog::error("博客数据导出失败：{}", blog_files.error());
            return std::unexpected{ blog_files.error() };
        }
        auto all_files = files;
        all_files.insert(all_files.end(), blog_files->begin(), blog_files->end());

        spdlog::info("博客数据导出完成（含 {} 个博客文件）。", blog_files->size());
        return zip_writer::build_zip(all_files);
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
