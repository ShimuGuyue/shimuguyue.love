/**
 * @file doc/blog_queries.cpp
 * @brief 博客（文档）数据库查询实现
 */

#include "doc/blog_queries.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>

#include "config/env.h"

namespace
{
    /**
     * @brief 将 int vector 转换为 PostgreSQL 数组字面量 "{1,2,3}"
     */
    auto join_ids(const std::vector<int>& ids) -> std::string
    {
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

    /**
     * @brief 校验单个字段值是否包含禁止的特殊字符。
     *        用于博客元数据字段的安全校验。
     * @param field_name 字段名称（仅用于构造错误消息）。
     * @param value      要校验的字段值。
     * @return std::nullopt 表示成功；否则返回错误消息。
     */
    static auto validate_field(std::string_view field_name, std::string_view value) -> std::optional<std::string>
    {
        // 禁止：HTML 实体字符、路径分隔符、空格、点号、常见标点
        constexpr std::string_view BAD = "<>&\"'\\|*?/ .!@#$%^&*()+=[]{};:'\",.<>?/`~";
        if (value.find_first_of(BAD) != std::string::npos)
            return std::string{ field_name } + " 含有特殊字符";
        return std::nullopt;
    }

    /**
     * @brief 批量校验所有博客元信息字段。
     *        依次校验 title, description, category_name, tag_names 中的每个标签、
     *        file_path_category, file_path_name。
     * @param title              博客标题
     * @param description        博客描述
     * @param category_name      分类名称
     * @param tag_names          标签名称列表
     * @param file_path_category 文件目录
     * @param file_path_name     文件名
     * @return std::nullopt 表示全部通过，否则返回第一个失败字段的错误消息。
    */
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



namespace doc
{

static std::string DOC_PATH;

void init()
{
    DOC_PATH = config::get_env("DOC_PATH");
}

auto doc_path() -> const std::string&
{
    return DOC_PATH;
}
    auto get_categories(pqxx::connection& conn) -> std::vector<Category>
    {
        spdlog::debug("正在从数据库获取分类列表...");
        pqxx::work txn{ conn };
        const auto rows = txn.exec(
            "SELECT id, name FROM categories ORDER BY id"
        );
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
        spdlog::debug("从数据库获取分类列表完成。");
        return result;
    }

    auto get_tags(
        pqxx::connection&       conn,
        const std::vector<int>& category_ids)
    -> std::vector<Tag>
    {
        spdlog::debug("正在从数据库获取标签列表...");
        pqxx::work txn{ conn };

        pqxx::result rows;
        if (category_ids.empty())
        {
            rows = txn.exec(
                "SELECT id, name, category_id FROM tags "
                "ORDER BY id"
            );
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
        spdlog::debug("从数据库获取标签列表完成。");
        return result;
    }

    auto get_blogs(
        pqxx::connection& conn,
        const BlogQuery&  query)
    -> std::vector<BlogItem>
    {
        spdlog::debug("正在从数据库获取博客列表...");
        pqxx::work txn{ conn };

        // 动态构建 SQL
        std::ostringstream sql;
        sql << "SELECT b.id, b.title, b.description, b.file_path, "
               "TO_CHAR(b.update_time, 'YYYY-MM-DD') AS update_time, "
               "c.name AS category_name "
               "FROM blogs b "
               "LEFT JOIN categories c ON c.id = b.category_id ";

        pqxx::params pq_params;
        bool has_where{ false };
        int param_idx{ 0 };

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
        // 使用 ILIKE 进行大小写不敏感模糊匹配
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

        // 统一按更新时间倒序排列
        sql << "ORDER BY b.update_time DESC";

        const auto rows = txn.exec(sql.str(), pq_params);

        // 所有博客的 ID
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
                const auto bid  = tr["blog_id"].as<int>();
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
        spdlog::debug("从数据库获取博客列表完成。");
        return result;
    }

    auto get_blog_by_file_path(
        pqxx::connection& conn,
        std::string_view  file_path)
    -> std::optional<BlogItem>
    {
        if (file_path.empty())
            return std::nullopt;

        spdlog::debug("正在从数据库按文件路径获取博客：{}", file_path);
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
        spdlog::debug("博客获取成功：{}", item.title);
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
        // 校验元信息字段
        if (auto err = validate_all_fields(title, description, category_name, tag_names, file_path_category, file_path_name); err)
        {
            spdlog::error("保存博客失败：{}", *err);
            return err;
        }

        // 拼接博客文件的相对路径："category/name"
        const std::string file_path = std::string{ file_path_category } + "/" + std::string{ file_path_name };

        pqxx::work txn{ conn };
        // 检查文件路径是否已被占用
        {
            auto r = txn.exec("SELECT 1 FROM blogs WHERE file_path = $1",
                              pqxx::params{ file_path });
            if (!r.empty())
            {
                spdlog::info("保存博客失败：路径 {} 已存在。", file_path);
                return std::string{ "博客路径已存在" };
            }
        }

        // 创建或查找分类
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

        // 创建或查找标签
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
                r = txn.exec(
                    "SELECT id FROM tags WHERE name = $1 AND category_id = $2",
                    pqxx::params{ tn, category_id });
                if (r.empty())
                {
                    spdlog::error("保存博客失败：创建标签 {} 失败。", tn);
                    return std::string{ "创建标签失败" };
                }
                tag_ids.push_back(r[0]["id"].as<int>());
            }
        }

        // 插入博客数据库记录
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

        // 关联博客与标签
        for (int tid : tag_ids)
        {
            txn.exec("INSERT INTO blog_tags (blog_id, tag_id) VALUES ($1, $2) "
                     "ON CONFLICT DO NOTHING",
                     pqxx::params{ blog_id, tid });
        }

        // 生成 Frontmatter 并写入 .md
        // 文件路径格式：{doc_path}/blogs/{category}/{name}.md
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

            std::filesystem::path out_path{ std::format("{}/blogs/{}.md", doc_path(), file_path) };
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
    // 校验 file_path
    if (file_path.empty())
    {
        spdlog::error("删除博客失败：缺少 file_path 参数。");
        return "缺少 file_path 参数";
    }

    pqxx::work txn{ conn };

    // 查找博客记录
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

    // 收集关联的标签 ID
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

    // 删除博客记录（CASCADE 自动清理 blog_tags 关联）
    txn.exec("DELETE FROM blogs WHERE id = $1", pqxx::params{ blog_id });
    // 清理孤立标签
    for (int tid : tag_ids)
    {
        const auto ref = txn.exec(
            "SELECT 1 FROM blog_tags WHERE tag_id = $1 LIMIT 1",
            pqxx::params{ tid }
        );
        if (ref.empty())
            txn.exec("DELETE FROM tags WHERE id = $1", pqxx::params{ tid });
    }

    // 清理孤立分类
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

    // 删除服务器上的 .md 文件及可能为空的父目录
    {
        std::error_code ec;
        std::filesystem::path md_path{ std::format("{}/blogs/{}.md", doc_path(), file_path) };
        std::filesystem::remove(md_path, ec);
        if (ec)
        {
            spdlog::error("删除博客文件失败：{} - {}", md_path.string(), ec.message());
            return "删除博客文件失败";
        }
        ec.clear();
        std::filesystem::remove(md_path.parent_path(), ec);
        if (ec)
        {
            spdlog::error("删除博客父目录失败：{} - {}", md_path.parent_path().string(), ec.message());
            return "删除博客文件失败";
        }
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
        // 校验元信息字段
        if (auto err = validate_all_fields(title, description, category_name, tag_names, file_path_category, file_path_name); err)
        {
            spdlog::error("更新博客失败：{}", *err);
            return err;
        }

        const std::string new_file_path{ std::format("{}/{}", file_path_category, file_path_name) };
        const bool path_changed = (old_file_path != new_file_path);

        pqxx::work txn{ conn };

        // 查找旧博客记录
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

        // 收集旧标签 ID
        const auto old_tags = txn.exec(
            "SELECT tag_id FROM blog_tags WHERE blog_id = $1",
            pqxx::params{ blog_id });
        std::vector<int> old_tag_ids;
        for (const auto& tr : old_tags)
        {
            old_tag_ids.push_back(tr["tag_id"].as<int>());
        }

        // 创建或查找新分类
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
                             pqxx::params{ std::string{ category_name } });
                if (r.empty())
                {
                    spdlog::error("更新博客失败：创建分类 {} 失败。", category_name);
                    return "创建分类失败";
                }
                category_id = r[0]["id"].as<int>();
            }
        }

        // 创建或查找新标签
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
                if (r.empty())
                {
                    spdlog::error("更新博客失败：创建标签 {} 失败。", tn);
                    return "创建标签失败";
                }
                new_tag_ids.push_back(r[0]["id"].as<int>());
            }
        }

        // 路径变更时检查新路径是否已被占用
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

        // 更新博客数据库记录
        std::string dt{ date };
        txn.exec(
            "UPDATE blogs SET title = $1, description = $2, content = $3, "
            "update_time = $4::date, category_id = $5, file_path = $6 WHERE id = $7",
            pqxx::params{ std::string{ title }, std::string{ description },
                          std::string{ content }, dt, category_id,
                          std::string{ new_file_path }, blog_id });

        // 重建博客-标签关联（先删后插）
        txn.exec("DELETE FROM blog_tags WHERE blog_id = $1", pqxx::params{ blog_id });
        for (int tid : new_tag_ids)
        {
            txn.exec("INSERT INTO blog_tags (blog_id, tag_id) VALUES ($1, $2) "
                     "ON CONFLICT DO NOTHING",
                     pqxx::params{blog_id, tid});
        }

        // 清理孤立标签
        for (int tid : old_tag_ids)
        {
            const auto ref = txn.exec(
                "SELECT 1 FROM blog_tags WHERE tag_id = $1 LIMIT 1",
                pqxx::params{ tid });
            if (ref.empty())
                txn.exec("DELETE FROM tags WHERE id = $1", pqxx::params{ tid });
        }

        // 清理未被引用的分类
        if (old_cat_id > 0 && old_cat_id != category_id)
        {
            const auto cat_ref = txn.exec(
                "SELECT 1 FROM blogs WHERE category_id = $1 LIMIT 1",
                pqxx::params{ old_cat_id });
            if (cat_ref.empty())
                txn.exec("DELETE FROM categories WHERE id = $1", pqxx::params{ old_cat_id });
        }

        txn.commit();

        // 如果文件路径变更，删除旧 .md 文件
        if (path_changed)
        {
            std::error_code ec;
            std::filesystem::path old_md{ std::format("{}/blogs/{}.md", doc_path(), old_file_path) };
            std::filesystem::remove(old_md, ec);
            if (ec)
            {
                spdlog::error("删除旧博客文件失败：{} - {}", old_md.string(), ec.message());
                return "删除旧博客文件失败";
            }
            ec.clear();
            std::filesystem::remove(old_md.parent_path(), ec);
            if (ec)
            {
                spdlog::error("删除旧博客父目录失败：{} - {}", old_md.parent_path().string(), ec.message());
                return "删除旧博客父目录失败";
            }
        }

        // 生成 Frontmatter 并写入新 .md 文件
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

            std::filesystem::path out_path{ std::format("{}/blogs/{}.md", doc_path(), new_file_path) };
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

} // namespace doc
