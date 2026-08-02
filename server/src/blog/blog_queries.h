/**
 * @file blog/blog_queries.h
 * @brief 博客相关数据库查询函数
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace blog
{

/**
 * @brief 分类信息。
 */
struct Category
{
    int         id;
    std::string name;
};

/**
 * @brief 标签信息。
 */
struct Tag
{
    int         id;
    std::string name;
    int         category_id;
};

/**
 * @brief 博客列表项。
 */
struct BlogItem
{
    int                        id;
    std::string                title;
    std::optional<std::string> description;
    std::optional<std::string> content;
    std::optional<std::string> file_path;
    std::string                update_time;
    std::optional<std::string> category;
    std::vector<std::string>   tags;
};

/**
 * @brief 博客列表查询参数。
 */
struct BlogQuery
{
    std::vector<int>            category_ids;
    std::vector<int>            tag_ids;
    std::optional<std::string>  search;
};

/**
 * @brief 查询所有分类。
 * @param conn 数据库连接。
 * @return 分类列表。
 */
[[nodiscard]] auto get_categories(pqxx::connection& conn) -> std::vector<Category>;

/**
 * @brief 查询标签列表，可按分类筛选（多选取并集）。
 * @param conn         数据库连接。
 * @param category_ids 分类 ID 列表，空则返回全部。
 * @return 标签列表。
 */
[[nodiscard]] auto get_tags(
    pqxx::connection&       conn,
    const std::vector<int>& category_ids)
-> std::vector<Tag>;

/**
 * @brief 查询博客列表，支持筛选和搜索。
 * @param conn  数据库连接。
 * @param query 筛选参数。
 * @return 博客列表项。
 */
[[nodiscard]] auto get_blogs(
    pqxx::connection& conn,
    const BlogQuery&  query)
-> std::vector<BlogItem>;

/**
 * @brief 根据文件路径查询单篇博客。
 * @param conn      数据库连接。
 * @param file_path 博客文件相对路径。
 * @return 成功返回 BlogItem，失败返回 std::nullopt。
 */
[[nodiscard]] auto get_blog_by_file_path(
    pqxx::connection& conn,
    std::string_view  file_path)
 -> std::optional<BlogItem>;

/**
 * @brief 删除博客及相关元数据，同时删除 .md 文件。
 *
 * 删除顺序：
 * 1. 删除博客记录（CASCADE 自动删除 blog_tags 关联）。
 * 2. 删除已无关联的标签。
 * 3. 删除已无博客的分类。
 * 4. 删除服务器上的 .md 文件。
 *
 * @param conn      数据库连接。
 * @param file_path 博客文件相对路径。
 * @return 错误消息字符串（空表示成功）。
 */
[[nodiscard]] auto delete_blog(
    pqxx::connection& conn,
    std::string_view  file_path)
-> std::string;

/**
 * @brief 保存博客及关联元数据，同时写入 .md 文件。
 * @param conn                数据库连接。
 * @param title               标题。
 * @param description         描述。
 * @param category_name       分类名（不存在则新建）。
 * @param tag_names           标签名列表（不存在则新建）。
 * @param file_path_category  文件路径分类目录部分。
 * @param file_path_name      文件路径文件名部分。
 * @param content             博客正文（Markdown）。
 * @param date                更新日期（YYYY-MM-DD 格式）。
 * @return 错误消息字符串（空表示成功）。
 */
[[nodiscard]] auto save_blog(
    pqxx::connection&               conn,
    std::string_view                title,
    std::string_view                description,
    std::string_view                category_name,
    const std::vector<std::string>& tag_names,
    std::string_view                file_path_category,
    std::string_view                file_path_name,
    std::string_view                content,
    std::string_view                date)
-> std::string;

/**
 * @brief 更新已有博客及关联元数据，同时覆盖 .md 文件。
 *        若 new_file_path 与 old_file_path 不同，会删除旧 md 并写入新路径。
 * @param conn                数据库连接。
 * @param title               标题。
 * @param description         描述。
 * @param category_name       分类名（不存在则新建）。
 * @param tag_names           标签名列表（不存在则新建）。
 * @param old_file_path       查找博客的旧文件路径。
 * @param file_path_category  新文件路径分类目录部分。
 * @param file_path_name      新文件路径文件名部分。
 * @param content             博客正文（Markdown）。
 * @param date                更新日期（YYYY-MM-DD 格式）。
 * @return 错误消息字符串（空表示成功）。
 */
[[nodiscard]] auto update_blog(
    pqxx::connection&               conn,
    std::string_view                title,
    std::string_view                description,
    std::string_view                category_name,
    const std::vector<std::string>& tag_names,
    std::string_view                old_file_path,
    std::string_view                file_path_category,
    std::string_view                file_path_name,
    std::string_view                content,
    std::string_view                date)
-> std::string;

} // namespace blog
