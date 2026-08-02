/**
 * @file blog/blog_queries.cpp
 * @brief 博客相关数据库查询实现
 */

#include "blog/blog_queries.h"
#include "md/markdown_parser.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>

namespace
{

/// 将 int vector 转换为 PostgreSQL 数组字面量 "{1,2,3}"
auto join_ids(const std::vector<int>& ids) -> std::string
{
    if (ids.empty())
        return "{}";

    std::ostringstream oss;
    oss << "{";
    for (std::size_t i{ 0 }; i < ids.size(); ++i)
    {
        if (i > 0)
            oss << ",";
        oss << ids[i];
    }
    oss << "}";
    return oss.str();
}

/// 校验单个字段，禁止所有标点 / 空格 / 特殊字符，返回错误消息（std::nullopt 表示通过）
static auto validate_field(std::string_view field_name, std::string_view value) -> std::optional<std::string>
{
    // 禁止：HTML 实体字符、路径分隔符、空格、点号、常见标点
    constexpr std::string_view BAD = "<>&\"'\\|*?/ .!@#$%^&*()+=[]{};:'\",.<>?/`~";

    if (value.find_first_of(BAD) != std::string::npos)
        return std::string{ field_name } + " 含有特殊字符";
    return std::nullopt;
}

/// 校验所有元信息字段，返回错误消息（std::nullopt 表示通过）
static auto validate_all_fields(
    std::string_view                title,
    std::string_view                description,
    std::string_view                category_name,
    const std::vector<std::string>& tag_names,
    std::string_view                file_path_category,
    std::string_view                file_path_name)
-> std::optional<std::string>
{
    if (auto err = validate_field("标题", title);                  err)
        return err;
    if (auto err = validate_field("描述", description);            err)
        return err;
    if (auto err = validate_field("分类", category_name);          err)
        return err;
    for (const auto& tag : tag_names)
    {
        if (auto err = validate_field("标签", tag);                err)
            return err;
    }
    if (auto err = validate_field("文件路径", file_path_category); err)
        return err;
    if (auto err = validate_field("文件路径", file_path_name);     err)
        return err;
    return std::nullopt;
}

} // namespace



namespace blog
{

auto get_categories(pqxx::connection& conn) -> std::vector<Category>
{
    spdlog::info("正在从数据库获取分类列表...");
    pqxx::work txn{ conn };
    const auto rows = txn.exec("SELECT id, name FROM categories ORDER BY id");
    txn.commit();

    std::vector<Category> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
    {
        result.push_back({
            row["id"]  .as<int>(),
            row["name"].as<std::string>()
        });
    }
    spdlog::info("从数据库获取分类列表完成。");
    return result;
}

auto get_tags(
    pqxx::connection&       conn,
    const std::vector<int>& category_ids)
-> std::vector<Tag>
{
    spdlog::info("正在从数据库获取标签列表...");
    pqxx::work txn{ conn };

    pqxx::result rows;
    if (category_ids.empty())
    {
        rows = txn.exec("SELECT id, name, category_id FROM tags ORDER BY id");
    }
    else
    {
        rows = txn.exec(
            "SELECT id, name, category_id FROM tags "
            "WHERE category_id = ANY($1) ORDER BY id",
            pqxx::params{ "{" + join_ids(category_ids) + "}" }
        );
    }
    txn.commit();

    std::vector<Tag> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
    {
        result.push_back({
            row["id"]         .as<int>(),
            row["name"]       .as<std::string>(),
            row["category_id"].as<int>()
        });
    }
    spdlog::info("从数据库获取标签列表完成。");
    return result;
}

auto get_blogs(
    pqxx::connection& conn,
    const BlogQuery&  query)
-> std::vector<BlogItem>
{
    spdlog::info("正在从数据库获取博客列表...");
    pqxx::work txn{ conn };

    // 动态构建 SQL
    std::ostringstream sql;
    sql << "SELECT b.id, b.title, b.description, b.file_path, "
           "TO_CHAR(b.update_time, 'YYYY-MM-DD') AS update_time, "
           "c.name AS category_name "
           "FROM blogs b "
           "LEFT JOIN categories c ON c.id = b.category_id ";

    pqxx::params pq_params;
    bool has_where = false;
    int param_idx = 0;

    // 分类筛选（多选取并集）
    if (!query.category_ids.empty())
    {
        sql << "WHERE b.category_id = ANY($" << ++param_idx << "::int[]) ";
        pq_params.append("{" + join_ids(query.category_ids) + "}");
        has_where = true;
    }

    // 标签筛选（多选取并集）
    if (!query.tag_ids.empty())
    {
        sql << (has_where ? "AND " : "WHERE ")
            << "b.id IN (SELECT blog_id FROM blog_tags "
            << "WHERE tag_id = ANY($" << ++param_idx << "::int[])) ";
        pq_params.append("{" + join_ids(query.tag_ids) + "}");
        has_where = true;
    }

    // 搜索（标题、描述、分类名、标签名）
    if (query.search && !query.search->empty())
    {
        spdlog::debug("博客搜索关键词：{}", *query.search);
        sql << (has_where ? "AND " : "WHERE ")
            << "(b.title ILIKE $" << ++param_idx
            << " OR b.description ILIKE $" << param_idx
            << " OR c.name ILIKE $" << param_idx
            << " OR b.id IN ("
            << "SELECT bt.blog_id FROM blog_tags bt "
            << "JOIN tags t ON t.id = bt.tag_id "
            << "WHERE t.name ILIKE $" << param_idx
            << ")) ";
        const auto pattern = "%" + *query.search + "%";
        pq_params.append(pattern);
    }

    sql << "ORDER BY b.update_time DESC";

    const auto rows = txn.exec(sql.str(), pq_params);

    std::vector<int> blog_ids;
    blog_ids.reserve(rows.size());

    std::vector<BlogItem> result;
    result.reserve(rows.size());

    for (const auto& row : rows)
    {
        const int id = row["id"].as<int>();
        blog_ids.push_back(id);

        BlogItem item;
        item.id          = id;
        item.title       = row["title"]      .as<std::string>();
        item.update_time = row["update_time"].as<std::string>();

        if (!row["description"].is_null())
            item.description = row["description"]  .as<std::string>();

        if (!row["file_path"].is_null())
            item.file_path   = row["file_path"]    .as<std::string>();

        if (!row["category_name"].is_null())
            item.category    = row["category_name"].as<std::string>();

        result.push_back(std::move(item));
    }

    // 批量查询标签
    if (!blog_ids.empty())
    {
        const auto tag_rows = txn.exec(
            "SELECT bt.blog_id, t.name "
            "FROM blog_tags bt "
            "JOIN tags t ON t.id = bt.tag_id "
            "WHERE bt.blog_id = ANY($1) "
            "ORDER BY t.name",
            pqxx::params{ "{" + join_ids(blog_ids) + "}" }
        );

        for (const auto& tr : tag_rows)
        {
            const int  bid  = tr["blog_id"].as<int>();
            const auto name = tr["name"]   .as<std::string>();
            for (auto& item : result)
            {
                if (item.id == bid)
                {
                    item.tags.push_back(name);
                    break;
                }
            }
        }
    }

    txn.commit();
    spdlog::info("从数据库获取博客列表完成。");
    return result;
}

auto get_blog_by_file_path(
    pqxx::connection& conn,
    std::string_view  file_path)
-> std::optional<BlogItem>
{
    if (file_path.empty())
        return std::nullopt;

    spdlog::info("正在从数据库按文件路径获取博客：{}", file_path);
    pqxx::work txn{ conn };

    const auto row = txn.exec(
        "SELECT b.id, b.title, b.description, b.content, b.file_path, "
        "TO_CHAR(b.update_time, 'YYYY-MM-DD') AS update_time, "
        "c.name AS category_name "
        "FROM blogs b "
        "LEFT JOIN categories c ON c.id = b.category_id "
        "WHERE b.file_path = $1",
        pqxx::params{ std::string{file_path} }
    );

    if (row.empty())
    {
        spdlog::info("博客不存在：{}", file_path);
        return std::nullopt;
    }

    BlogItem item;
    item.id          = row[0]["id"]         .as<int>();
    item.title       = row[0]["title"]      .as<std::string>();
    item.update_time = row[0]["update_time"].as<std::string>();

    if (!row[0]["description"]  .is_null())
        item.description = row[0]["description"]  .as<std::string>();

    if (!row[0]["content"]      .is_null())
        item.content     = row[0]["content"]      .as<std::string>();

    if (!row[0]["category_name"].is_null())
        item.category    = row[0]["category_name"].as<std::string>();

    if (!row[0]["file_path"]    .is_null())
        item.file_path   = row[0]["file_path"]    .as<std::string>();

    // 查询标签
    const auto tag_rows = txn.exec(
        "SELECT t.name "
        "FROM blog_tags bt "
        "JOIN tags t ON t.id = bt.tag_id "
        "WHERE bt.blog_id = $1 "
        "ORDER BY t.name",
        pqxx::params{ item.id }
    );

    for (const auto& tr : tag_rows)
    {
        item.tags.push_back(tr["name"].as<std::string>());
    }

    txn.commit();
    spdlog::info("博客获取成功：{}", item.title);
    return item;
}

auto save_blog(
    pqxx::connection&               conn,
    std::string_view                title,
    std::string_view                description,
    std::string_view                category_name,
    const std::vector<std::string>& tag_names,
    std::string_view                file_path_category,
    std::string_view                file_path_name,
    std::string_view                content,
    std::string_view                date)
 -> std::optional<std::string>
{
    spdlog::info("正在保存博客 {}...", title);
    if (auto err = validate_all_fields(title, description, category_name, tag_names, file_path_category, file_path_name); err)
    {
        spdlog::error("保存博客失败：{}", *err);
        return err;
    }

    const std::string file_path = std::string{ file_path_category } + "/" + std::string{ file_path_name };

    pqxx::work txn{ conn };

    {
        auto r = txn.exec("SELECT 1 FROM blogs WHERE file_path = $1",
                          pqxx::params{ file_path });
        if (!r.empty())
        {
            spdlog::error("保存博客失败：路径 {} 已存在。", file_path);
            return std::string{ "博客路径已存在" };
        }
    }

    int category_id{ 0 };
    {
        pqxx::result r = txn.exec(
            "INSERT INTO categories (name) VALUES ($1) "
            "ON CONFLICT (name) DO NOTHING RETURNING id",
            pqxx::params{ std::string{category_name} });
        if (!r.empty())
        {
            category_id = r[0]["id"].as<int>();
        }
        else
        {
            r = txn.exec("SELECT id FROM categories WHERE name = $1",
                         pqxx::params{ std::string{category_name} });
            if (r.empty())
            {
                spdlog::error("保存博客失败：创建分类 {} 失败。", category_name);
                return std::string{ "创建分类失败" };
            }
            category_id = r[0]["id"].as<int>();
        }
    }

    std::vector<int> tag_ids;
    for (const auto& tn : tag_names)
    {
        pqxx::result r = txn.exec(
            "INSERT INTO tags (name, category_id) VALUES ($1, $2) "
            "ON CONFLICT (name, category_id) DO NOTHING RETURNING id",
            pqxx::params{ tn, category_id });
        if (!r.empty())
        {
            tag_ids.push_back(r[0]["id"].as<int>());
        }
        else
        {
            r = txn.exec("SELECT id FROM tags WHERE name = $1 AND category_id = $2",
                         pqxx::params{ tn, category_id });
            if (!r.empty())
                tag_ids.push_back(r[0]["id"].as<int>());
        }
    }

    std::string dt{ date };

    pqxx::result r = txn.exec(
        "INSERT INTO blogs (title, description, content, file_path, "
        "update_time, category_id) "
        "VALUES ($1, $2, $3, $4, $5::date, $6) RETURNING id",
        pqxx::params{ std::string{title}, std::string{description},
                      std::string{content}, file_path, dt, category_id });
    if (r.empty())
    {
        spdlog::error("保存博客失败：插入数据库记录失败。");
        return std::string{ "创建博客记录失败" };
    }

    const int blog_id = r[0]["id"].as<int>();

    for (int tid : tag_ids)
    {
        txn.exec("INSERT INTO blog_tags (blog_id, tag_id) VALUES ($1, $2) "
                 "ON CONFLICT DO NOTHING",
                 pqxx::params{ blog_id, tid });
    }

    {
        std::ostringstream fm;
        fm << "---\n";
        fm << "title: " << title << "\n";
        fm << "description: " << description << "\n";
        fm << "category: " << category_name << "\n";
        fm << "tags: [";
        for (std::size_t i{ 0 }; i < tag_names.size(); ++i)
        {
            if (i > 0)
                fm << ", ";
            fm << tag_names[i];
        }
        fm << "]\n";
        fm << "update_time: " << date << "\n";
        fm << "file_path: " << file_path << "\n";
        fm << "---\n\n";
        fm << content;

        std::filesystem::path out_path{ std::format("{}/blogs/{}.md",
                                                    md::doc_path(), file_path) };
        std::filesystem::create_directories(out_path.parent_path());
        std::ofstream ofs{ out_path, std::ios::binary };
        if (!ofs)
        {
            spdlog::error("保存博客失败：写入文件 {} 失败。", out_path.string());
            return std::string{ "写入文件失败！" };
        }
        ofs << fm.str();
        ofs.close();
    }

    txn.commit();
    spdlog::info("博客保存成功。");
    return std::nullopt;
}

auto delete_blog(
    pqxx::connection& conn,
    std::string_view  file_path)
-> std::optional<std::string>
{
    spdlog::info("正在删除博客 {}...", file_path);
    if (file_path.empty())
    {
        spdlog::error("删除博客失败：缺少 file_path 参数。");
        return "缺少 file_path 参数";
    }

    pqxx::work txn{ conn };

    const auto blog_row = txn.exec(
        "SELECT id, category_id FROM blogs WHERE file_path = $1",
        pqxx::params{ std::string{ file_path } }
    );
    if (blog_row.empty())
    {
        spdlog::error("删除博客失败：{} 不存在。", file_path);
        return "博客不存在";
    }

    const int blog_id     = blog_row[0]["id"].as<int>();
    const int category_id = blog_row[0]["category_id"].is_null()
                          ? 0
                          : blog_row[0]["category_id"].as<int>();

    const auto tag_rows = txn.exec(
        "SELECT tag_id FROM blog_tags WHERE blog_id = $1",
        pqxx::params{ blog_id }
    );
    std::vector<int> tag_ids;
    tag_ids.reserve(tag_rows.size());
    for (const auto& tr : tag_rows)
    {
        tag_ids.push_back(tr["tag_id"].as<int>());
    }

    txn.exec("DELETE FROM blogs WHERE id = $1", pqxx::params{ blog_id });

    for (int tid : tag_ids)
    {
        const auto ref = txn.exec(
            "SELECT 1 FROM blog_tags WHERE tag_id = $1 LIMIT 1",
            pqxx::params{ tid }
        );
        if (ref.empty())
            txn.exec("DELETE FROM tags WHERE id = $1", pqxx::params{ tid });
    }

    if (category_id > 0)
    {
        const auto cat_ref = txn.exec(
            "SELECT 1 FROM blogs WHERE category_id = $1 LIMIT 1",
            pqxx::params{ category_id }
        );
        if (cat_ref.empty())
            txn.exec("DELETE FROM categories WHERE id = $1", pqxx::params{ category_id });
    }

    txn.commit();

    {
        std::error_code ec;
        std::filesystem::path md_path{ std::format("{}/blogs/{}.md", md::doc_path(), file_path) };
        std::filesystem::remove(md_path, ec);
        std::filesystem::remove(md_path.parent_path(), ec);
        if (ec)
            spdlog::error("删除博客文件失败：{} - {}", md_path.string(), ec.message());
    }

    spdlog::info("博客删除成功。");
    return std::nullopt;
}

auto update_blog(
    pqxx::connection&              conn,
    std::string_view               title,
    std::string_view               description,
    std::string_view               category_name,
    const std::vector<std::string>& tag_names,
    std::string_view               old_file_path,
    std::string_view               file_path_category,
    std::string_view               file_path_name,
    std::string_view               content,
    std::string_view               date)
 -> std::optional<std::string>
{
    spdlog::info("正在更新博客 {}...", title);
    if (auto err = validate_all_fields(title, description, category_name, tag_names, file_path_category, file_path_name); err)
    {
        spdlog::error("更新博客失败：{}", *err);
        return err;
    }

    const std::string new_file_path{ std::format("{}/{}", file_path_category, file_path_name) };
    const bool path_changed = (old_file_path != new_file_path);

    pqxx::work txn{ conn };

    const auto blog_row = txn.exec(
        "SELECT id, category_id FROM blogs WHERE file_path = $1",
        pqxx::params{ std::string{old_file_path} });
    if (blog_row.empty())
    {
        spdlog::error("更新博客失败：{} 不存在。", old_file_path);
        return "博客不存在";
    }

    const int blog_id    = blog_row[0]["id"].as<int>();
    const int old_cat_id = blog_row[0]["category_id"].is_null()
                         ? 0
                         : blog_row[0]["category_id"].as<int>();

    const auto old_tags = txn.exec(
        "SELECT tag_id FROM blog_tags WHERE blog_id = $1",
        pqxx::params{ blog_id });
    std::vector<int> old_tag_ids;
    for (const auto& tr : old_tags)
    {
        old_tag_ids.push_back(tr["tag_id"].as<int>());
    }

    int category_id{ 0 };
    {
        pqxx::result r = txn.exec(
            "INSERT INTO categories (name) VALUES ($1) "
            "ON CONFLICT (name) DO NOTHING RETURNING id",
            pqxx::params{ std::string{category_name} });
        if (!r.empty())
        {
            category_id = r[0]["id"].as<int>();
        }
        else
        {
            r = txn.exec("SELECT id FROM categories WHERE name = $1",
                         pqxx::params{ std::string{category_name} });
            if (r.empty())
            {
                spdlog::error("更新博客失败：创建分类 {} 失败。", category_name);
                return "创建分类失败";
            }
            category_id = r[0]["id"].as<int>();
        }
    }

    std::vector<int> new_tag_ids;
    for (const auto& tn : tag_names)
    {
        pqxx::result r = txn.exec(
            "INSERT INTO tags (name, category_id) VALUES ($1, $2) "
            "ON CONFLICT (name, category_id) DO NOTHING RETURNING id",
            pqxx::params{ tn, category_id });
        if (!r.empty())
        {
            new_tag_ids.push_back(r[0]["id"].as<int>());
        }
        else
        {
            r = txn.exec("SELECT id FROM tags WHERE name = $1 AND category_id = $2",
                         pqxx::params{ tn, category_id });
            if (!r.empty())
                new_tag_ids.push_back(r[0]["id"].as<int>());
        }
    }

    if (path_changed)
    {
        auto dup = txn.exec("SELECT 1 FROM blogs WHERE file_path = $1",
                            pqxx::params{ std::string{new_file_path} });
        if (!dup.empty())
        {
            spdlog::error("更新博客失败：新路径 {} 已被占用。", new_file_path);
            return "新文件路径已被其他博客占用";
        }
    }

    std::string dt{ date };
    txn.exec(
        "UPDATE blogs SET title = $1, description = $2, content = $3, "
        "update_time = $4::date, category_id = $5, file_path = $6 WHERE id = $7",
        pqxx::params{ std::string{title}, std::string{description},
                      std::string{content}, dt, category_id,
                      std::string{new_file_path}, blog_id });

    txn.exec("DELETE FROM blog_tags WHERE blog_id = $1", pqxx::params{ blog_id });
    for (int tid : new_tag_ids)
    {
        txn.exec("INSERT INTO blog_tags (blog_id, tag_id) VALUES ($1, $2) "
                 "ON CONFLICT DO NOTHING",
                 pqxx::params{blog_id, tid});
    }

    for (int tid : old_tag_ids)
    {
        const auto ref = txn.exec(
            "SELECT 1 FROM blog_tags WHERE tag_id = $1 LIMIT 1",
            pqxx::params{ tid });
        if (ref.empty())
            txn.exec("DELETE FROM tags WHERE id = $1", pqxx::params{ tid });
    }

    if (old_cat_id > 0 && old_cat_id != category_id)
    {
        const auto cat_ref = txn.exec(
            "SELECT 1 FROM blogs WHERE category_id = $1 LIMIT 1",
            pqxx::params{ old_cat_id });
        if (cat_ref.empty())
            txn.exec("DELETE FROM categories WHERE id = $1", pqxx::params{ old_cat_id });
    }

    txn.commit();

    if (path_changed)
    {
        std::error_code ec;
        std::filesystem::path old_md{ std::format("{}/blogs/{}.md", md::doc_path(), old_file_path) };
        std::filesystem::remove(old_md, ec);
        std::filesystem::remove(old_md.parent_path(), ec);
        if (ec)
            spdlog::error("删除旧博客文件失败：{} - {}", old_md.string(), ec.message());
    }

    {
        std::ostringstream fm;
        fm << "---\n";
        fm << "title: " << title << "\n";
        fm << "description: " << description << "\n";
        fm << "category: " << category_name << "\n";
        fm << "tags: [";
        for (std::size_t i{ 0 }; i < tag_names.size(); ++i)
        {
            if (i > 0)
                fm << ", ";
            fm << tag_names[i];
        }
        fm << "]\n";
        fm << "update_time: " << date << "\n";
        fm << "file_path: " << new_file_path << "\n";
        fm << "---\n\n";
        fm << content;

        std::filesystem::path out_path{ std::format("{}/blogs/{}.md", md::doc_path(), new_file_path) };
        std::filesystem::create_directories(out_path.parent_path());
        std::ofstream ofs{ out_path, std::ios::binary };
        if (!ofs)
        {
            spdlog::error("更新博客失败：写入文件 {} 失败。", out_path.string());
            return "写入文件失败！";
        }
        ofs << fm.str();
    }

    spdlog::info("博客更新成功。");
    return std::nullopt;
}

} // namespace blog
