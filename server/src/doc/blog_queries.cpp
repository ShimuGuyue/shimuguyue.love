/**
 * @file doc/blog_queries.cpp
 * @brief 博客（文档）数据库查询实现
 */

#include "doc/blog_queries.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

#include "config/config.h"

namespace
{
    /**
     * @brief 判断字符串是否为合法的 YYYY-MM-DD 日期。
     * @param date 待校验的日期字符串。
     * @return 合法返回 true，否则返回 false。
     */
    auto is_valid_date(const std::string& date) -> bool
    {
        std::tm parsed{};
        std::istringstream ss{ date };
        ss >> std::get_time(&parsed, "%Y-%m-%d");
        if (ss.fail())
            return false;
        std::ostringstream os;
        os << std::put_time(&parsed, "%Y-%m-%d");
        return os.str() == date;
    }

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
     * @brief 博客筛选条件（WHERE 片段 + 绑定参数）。
     */
    struct BlogFilter
    {
        std::string  where;       ///< 形如 "WHERE ... AND ..."；无筛选时为空串
        pqxx::params params;      ///< 与占位符顺序一致的查询参数
        int          param_count; ///< 占位符个数，供 LIMIT / OFFSET 继续编号
    };

    /**
     * @brief 构建博客筛选 SQL 片段。
     *
     * 分类 / 标签筛选与关键词搜索的 WHERE 条件集中在此，
     * 供列表查询与计数查询复用，保证两者筛选口径一致。
     *
     * @param query 博客查询参数（page / page_size 不参与筛选）。
     * @return WHERE 片段与对应参数。
     */
    auto build_blog_filter(const doc::BlogQuery& query) -> BlogFilter
    {
        BlogFilter filter{ "", pqxx::params{}, 0 };
        std::ostringstream sql;
        bool has_where{ false };
        int  param_idx{ 0 };

        // 分类筛选（多选取并集）
        if (!query.category_ids.empty())
        {
            sql << "WHERE b.id IN (SELECT blog_id FROM blog_categories_relations "
                << "WHERE category_id = ANY($" << ++param_idx << "::int[])) ";
            filter.params.append("{" + join_ids(query.category_ids) + "}");
            has_where = true;
        }

        // 标签筛选（多选取并集）
        if (!query.tag_ids.empty())
        {
            sql << (has_where ? "AND " : "WHERE ")
                << "b.id IN (SELECT blog_id FROM blog_tag_relations "
                << "WHERE tag_id = ANY($" << ++param_idx << "::int[])) ";
            filter.params.append("{" + join_ids(query.tag_ids) + "}");
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
                << " OR b.id IN ("
                << "SELECT bc.blog_id FROM blog_categories_relations bc "
                << "JOIN blog_categories c ON c.id = bc.category_id "
                << "WHERE c.name ILIKE $" << param_idx
                << ") OR b.id IN ("
                << "SELECT bt.blog_id FROM blog_tag_relations bt "
                << "JOIN blog_tags t ON t.id = bt.tag_id "
                << "WHERE t.name ILIKE $" << param_idx
                << ")) ";
            filter.params.append("%" + *query.search + "%");
        }

        filter.where       = sql.str();
        filter.param_count = param_idx;
        return filter;
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
     *        依次校验 title, description, category_names, tag_names 中的每个条目、
     *        file_path_category, file_path_name。
     * @param title              博客标题
     * @param description        博客描述
     * @param category_names     分类名称列表
     * @param tag_names          标签名称列表
     * @param file_path_category 文件路径分类部分
     * @param file_path_name     文件路径文件名部分
     * @return std::nullopt 表示全部通过，否则返回第一个失败字段的错误消息。
     */
    static auto validate_all_fields(
        std::string_view                title,
        std::string_view                description,
        const std::vector<std::string>& category_names,
        const std::vector<std::string>& tag_names,
        std::string_view                file_path_category,
        std::string_view                file_path_name)
    -> std::optional<std::string>
    {
        if (auto err = validate_field("标题", title))
            return err;
        if (auto err = validate_field("描述", description))
            return err;
        for (const auto& category : category_names)
        {
            if (auto err = validate_field("分类", category))
                return err;
        }
        for (const auto& tag : tag_names)
        {
            if (auto err = validate_field("标签", tag))
                return err;
        }
        if (auto err = validate_field("文件路径", file_path_category))
            return err;
        if (auto err = validate_field("文件路径", file_path_name))
            return err;
        return std::nullopt;
    }

    /**
     * @brief 按文件路径查找博客 ID。
     * @param txn       当前事务。
     * @param file_path 博客文件相对路径。
     * @return 找到返回博客 ID；未找到返回 std::nullopt。
     */
    auto find_blog_row(pqxx::work& txn, std::string_view file_path) -> std::optional<int>
    {
        const auto row = txn.exec(
            "SELECT id FROM blogs WHERE file_path = $1",
            pqxx::params{ std::string{ file_path } }
        );
        if (row.empty())
            return std::nullopt;
        return row[0]["id"].as<int>();
    }

    /**
     * @brief 查询博客关联的全部标签 ID。
     * @param txn     当前事务。
     * @param blog_id 博客 ID。
     * @return 标签 ID 列表。
     */
    auto get_blog_tag_ids(pqxx::work& txn, int blog_id) -> std::vector<int>
    {
        const auto rows = txn.exec(
            "SELECT tag_id FROM blog_tag_relations WHERE blog_id = $1",
            pqxx::params{ blog_id }
        );

        std::vector<int> ids;
        ids.reserve(rows.size());
        for (const auto& row : rows)
        {
            ids.push_back(row["tag_id"].as<int>());
        }
        return ids;
    }

    /**
     * @brief 查询博客关联的全部分类 ID。
     * @param txn     当前事务。
     * @param blog_id 博客 ID。
     * @return 分类 ID 列表。
     */
    auto get_blog_category_ids(pqxx::work& txn, int blog_id) -> std::vector<int>
    {
        const auto rows = txn.exec(
            "SELECT category_id FROM blog_categories_relations WHERE blog_id = $1",
            pqxx::params{ blog_id }
        );

        std::vector<int> ids;
        ids.reserve(rows.size());
        for (const auto& row : rows)
        {
            ids.push_back(row["category_id"].as<int>());
        }
        return ids;
    }

    /**
     * @brief 刷新指定分类与标签的博客计数。
     * @details 分类 / 标签的 count 即“属于该分类 / 使用该标签的博客数”
     *          在博客关联发生变化后调用：按实际关联重算而非手动修改，确保正确性。
     * @param txn          当前事务。
     * @param category_ids 需要重算的分类 ID 列表。
     * @param tag_ids      需要重算的标签 ID 列表。
     */
    void refresh_category_tag_counts(
        pqxx::work&             txn,
        const std::vector<int>& category_ids,
        const std::vector<int>& tag_ids)
    {
        if (!category_ids.empty())
        {
            txn.exec(
                "UPDATE blog_categories c SET count = ("
                "SELECT COUNT(*) FROM blog_categories_relations r WHERE r.category_id = c.id) "
                "WHERE c.id = ANY($1::int[])",
                pqxx::params{ "{" + join_ids(category_ids) + "}" }
            );
        }

        if (!tag_ids.empty())
        {
            txn.exec(
                "UPDATE blog_tags t SET count = ("
                "SELECT COUNT(*) FROM blog_tag_relations r WHERE r.tag_id = t.id) "
                "WHERE t.id = ANY($1::int[])",
                pqxx::params{ "{" + join_ids(tag_ids) + "}" }
            );
        }
    }

    /**
     * @brief 批量维护分类-标签关联计数。
     * @details 先确保关联行存在（缺失时以 0 插入），再按 delta 增减计数。
     * @param txn    当前事务。
     * @param pairs  (分类 ID, 标签 ID) 组合列表。
     * @param delta  计数增量（+1 / -1）。
     */
    void apply_category_tag_deltas(
        pqxx::work&                             txn,
        const std::vector<std::pair<int, int>>& pairs,
        int                                     delta)
    {
        for (const auto& [category_id, tag_id] : pairs)
        {
            txn.exec(
                "INSERT INTO category_tag_relations (category_id, tag_id, count) "
                "VALUES ($1, $2, 0) ON CONFLICT DO NOTHING",
                pqxx::params{ category_id, tag_id }
            );
            txn.exec(
                "UPDATE category_tag_relations SET count = count + $3 "
                "WHERE category_id = $1 AND tag_id = $2",
                pqxx::params{ category_id, tag_id, delta }
            );
        }
    }

    /**
     * @brief 由分类 ID 列表与标签 ID 列表拼出全部 (分类, 标签) 组合。
     * @details 用双重循环直接拼出所有分类 ID 和标签 ID 的组合对。
     * @param category_ids 分类 ID 列表。
     * @param tag_ids      标签 ID 列表。
     * @return (分类 ID, 标签 ID) 组合列表。
     */
    auto make_category_tag_pairs(
        const std::vector<int>& category_ids,
        const std::vector<int>& tag_ids)
    -> std::vector<std::pair<int, int>>
    {
        std::vector<std::pair<int, int>> pairs;
        pairs.reserve(category_ids.size() * tag_ids.size());

        for (const int category_id : category_ids)
        {
            for (const int tag_id : tag_ids)
            {
                pairs.emplace_back(category_id, tag_id);
            }
        }
        return pairs;
    }

    /**
     * @brief 维护“博客新增归属”带来的分类-标签计数（+1）。
     * @details 由传入的分类 / 标签 ID 双重循环拼出组合，每个组合累加 1。
     * @param txn          当前事务。
     * @param category_ids 该博客的分类 ID 列表。
     * @param tag_ids      该博客的标签 ID 列表。
     */
    void add_blog_category_tag_counts(
        pqxx::work&             txn,
        const std::vector<int>& category_ids,
        const std::vector<int>& tag_ids)
    {
        apply_category_tag_deltas(txn, make_category_tag_pairs(category_ids, tag_ids), 1);
    }

    /**
     * @brief 维护“博客移除归属”带来的分类-标签计数（-1）。
     * @details 由传入的分类 / 标签 ID 双重循环拼出组合，每个组合累减 1。
     * @param txn          当前事务。
     * @param category_ids 该博客此前的分类 ID 列表。
     * @param tag_ids      该博客此前的标签 ID 列表。
     */
    void remove_category_tag_counts(
        pqxx::work&             txn,
        const std::vector<int>& category_ids,
        const std::vector<int>& tag_ids)
    {
        apply_category_tag_deltas(txn, make_category_tag_pairs(category_ids, tag_ids), -1);
    }

    /**
     * @brief 用数据库行填充博客列表项。
     * @param item 待填充的博客列表项。
     * @param row  查询结果行。
     */
    void fill_blog_item(doc::BlogItem& item, const pqxx::row_ref& row)
    {
        item.id          = row["id"]         .as<int>();
        item.title       = row["title"]      .as<std::string>();
        item.update_time = row["update_time"].as<std::string>();

        if (!row["description"].is_null())
            item.description = row["description"].as<std::string>();

        if (!row["content"].is_null())
            item.content = row["content"].as<std::string>();

        if (!row["file_path"].is_null())
            item.file_path = row["file_path"].as<std::string>();
    }

    /**
     * @brief 批量把标签名挂到对应博客列表项上。
     * @details 按 blog_id 建索引后再拼接，避免逐行线性查找。
     * @param txn      当前事务。
     * @param blog_ids 博客 ID 列表；为空时不查询。
     * @param items    待填充的博客列表项。
     */
    void attach_tag_names(
        pqxx::work&                 txn,
        const std::vector<int>&     blog_ids,
        std::vector<doc::BlogItem>& items)
    {
        if (blog_ids.empty())
            return;

        std::unordered_map<int, std::size_t> index;
        index.reserve(items.size());
        for (std::size_t i{ 0 }; i < items.size(); ++i)
        {
            index[items[i].id] = i;
        }

        const auto rows = txn.exec(
            "SELECT bt.blog_id, t.name "
            "FROM blog_tag_relations bt "
            "JOIN blog_tags t ON t.id = bt.tag_id "
            "WHERE bt.blog_id = ANY($1) "
            "ORDER BY t.name",
            pqxx::params{ "{" + join_ids(blog_ids) + "}" }
        );

        for (const auto& row : rows)
        {
            const auto it = index.find(row["blog_id"].as<int>());
            if (it != index.end())
                items[it->second].tags.push_back(row["name"].as<std::string>());
        }
    }

    /**
     * @brief 批量把分类名挂到对应博客列表项上。
     * @details 按 blog_id 建索引后再拼接，避免逐行线性查找。
     * @param txn      当前事务。
     * @param blog_ids 博客 ID 列表；为空时不查询。
     * @param items    待填充的博客列表项。
     */
    void attach_category_names(
        pqxx::work&                 txn,
        const std::vector<int>&     blog_ids,
        std::vector<doc::BlogItem>& items)
    {
        if (blog_ids.empty())
            return;

        std::unordered_map<int, std::size_t> index;
        index.reserve(items.size());
        for (std::size_t i{ 0 }; i < items.size(); ++i)
        {
            index[items[i].id] = i;
        }

        const auto rows = txn.exec(
            "SELECT bc.blog_id, c.name "
            "FROM blog_categories_relations bc "
            "JOIN blog_categories c ON c.id = bc.category_id "
            "WHERE bc.blog_id = ANY($1) "
            "ORDER BY c.name",
            pqxx::params{ "{" + join_ids(blog_ids) + "}" }
        );

        for (const auto& row : rows)
        {
            const auto it = index.find(row["blog_id"].as<int>());
            if (it != index.end())
                items[it->second].categories.push_back(row["name"].as<std::string>());
        }
    }

    /**
     * @brief 检查博客文件路径是否已被占用。
     * @param txn       当前事务。
     * @param file_path 博客文件相对路径。
     * @return true 表示已占用。
     */
    auto is_file_path_taken(pqxx::work& txn, std::string_view file_path) -> bool
    {
        const auto r = txn.exec(
            "SELECT 1 FROM blogs WHERE file_path = $1",
            pqxx::params{ std::string{ file_path } }
        );
        return !r.empty();
    }

    /**
     * @brief 创建分类；已存在时不插入。
     * @param txn  当前事务。
     * @param name 分类名。
     * @return 新分类 ID；已存在时返回 std::nullopt。
     */
    auto create_category(pqxx::work& txn, std::string_view name) -> std::optional<int>
    {
        const auto r = txn.exec(
            "INSERT INTO blog_categories (name) VALUES ($1) "
            "ON CONFLICT (name) DO NOTHING RETURNING id",
            pqxx::params{ std::string{ name } }
        );
        if (r.empty())
            return std::nullopt;
        return r[0]["id"].as<int>();
    }

    /**
     * @brief 按名称查找分类。
     * @param txn  当前事务。
     * @param name 分类名。
     * @return 分类 ID；不存在时返回 std::nullopt。
     */
    auto find_category(pqxx::work& txn, std::string_view name) -> std::optional<int>
    {
        const auto r = txn.exec(
            "SELECT id FROM blog_categories WHERE name = $1",
            pqxx::params{ std::string{ name } }
        );
        if (r.empty())
            return std::nullopt;
        return r[0]["id"].as<int>();
    }

    /**
     * @brief 创建标签；已存在时不插入。
     * @param txn  当前事务。
     * @param name 标签名。
     * @return 新标签 ID；已存在时返回 std::nullopt。
     */
    auto create_tag(pqxx::work& txn, std::string_view name) -> std::optional<int>
    {
        const auto r = txn.exec(
            "INSERT INTO blog_tags (name) VALUES ($1) "
            "ON CONFLICT (name) DO NOTHING RETURNING id",
            pqxx::params{ name }
        );
        if (r.empty())
            return std::nullopt;
        return r[0]["id"].as<int>();
    }

    /**
     * @brief 按名称查找标签。
     * @param txn  当前事务。
     * @param name 标签名。
     * @return 标签 ID；不存在时返回 std::nullopt。
     */
    auto find_tag(pqxx::work& txn, std::string_view name) -> std::optional<int>
    {
        const auto r = txn.exec(
            "SELECT id FROM blog_tags WHERE name = $1",
            pqxx::params{ name }
        );
        if (r.empty())
            return std::nullopt;
        return r[0]["id"].as<int>();
    }

    /**
     * @brief 关联博客与标签。
     * @param txn     当前事务。
     * @param blog_id 博客 ID。
     * @param tag_ids 标签 ID 列表。
     */
    void link_blog_tags(pqxx::work& txn, int blog_id, const std::vector<int>& tag_ids)
    {
        for (int tid : tag_ids)
        {
            txn.exec("INSERT INTO blog_tag_relations (blog_id, tag_id) VALUES ($1, $2) "
                     "ON CONFLICT DO NOTHING",
                     pqxx::params{ blog_id, tid });
        }
    }

    /**
     * @brief 关联博客与分类。
     * @param txn          当前事务。
     * @param blog_id      博客 ID。
     * @param category_ids 分类 ID 列表。
     */
    void link_blog_categories(pqxx::work& txn, int blog_id, const std::vector<int>& category_ids)
    {
        for (int cid : category_ids)
        {
            txn.exec("INSERT INTO blog_categories_relations (blog_id, category_id) VALUES ($1, $2) "
                     "ON CONFLICT DO NOTHING",
                     pqxx::params{ blog_id, cid });
        }
    }

    /**
     * @brief 把分类名解析为分类 ID，不存在的分类随即创建。
     * @param txn   当前事务。
     * @param names 分类名列表。
     * @return 与输入顺序一致的分类 ID 列表；
     *         解析失败返回 std::nullopt。
     */
    auto resolve_category_ids(pqxx::work& txn, const std::vector<std::string>& names)
    -> std::optional<std::vector<int>>
    {
        std::vector<int> ids;
        ids.reserve(names.size());
        for (const auto& name : names)
        {
            auto id = create_category(txn, name);
            if (!id)
                id = find_category(txn, name);
            if (!id)
                return std::nullopt;
            ids.push_back(*id);
        }
        return ids;
    }

    /**
     * @brief 把标签名解析为标签 ID，不存在的标签随即创建。
     * @param txn   当前事务。
     * @param names 标签名列表。
     * @return 与输入顺序一致的标签 ID 列表；
     *         解析失败返回 std::nullopt。
     */
    auto resolve_tag_ids(pqxx::work& txn, const std::vector<std::string>& names)
    -> std::optional<std::vector<int>>
    {
        std::vector<int> ids;
        ids.reserve(names.size());
        for (const auto& name : names)
        {
            auto id = create_tag(txn, name);
            if (!id)
                id = find_tag(txn, name);
            if (!id)
                return std::nullopt;
            ids.push_back(*id);
        }
        return ids;
    }

    /**
     * @brief 生成 Frontmatter 并写入博客 .md 文件。
     * @param blog 博客内容与元信息（file_path 为完整相对路径）。
     * @return std::nullopt 表示成功；否则返回错误消息。
     */
    auto write_blog_md(const doc::BlogContent& blog)
    -> std::optional<std::string>
    {
        std::ostringstream fm;
        fm << "---\n";
        fm << "title: " << blog.title << "\n";
        fm << "description: " << blog.description << "\n";
        fm << "categories: [";
        for (std::size_t i{ 0 }; i < blog.categories.size(); ++i)
        {
            if (i > 0)
                fm << ", ";
            fm << blog.categories[i];
        }
        fm << "]\n";
        fm << "tags: [";
        for (std::size_t i{ 0 }; i < blog.tags.size(); ++i)
        {
            if (i > 0)
                fm << ", ";
            fm << blog.tags[i];
        }
        fm << "]\n";
        fm << "update_time: " << blog.date << "\n";
        fm << "file_path: " << blog.file_path << "\n";
        fm << "---\n\n";
        fm << blog.content;

        std::filesystem::path out_path{
            std::filesystem::path{ config::config()["FILE_PATH"] } / "blogs"
            / (blog.file_path + ".md")
        };
        std::filesystem::create_directories(out_path.parent_path());
        std::ofstream ofs{ out_path, std::ios::binary };
        if (!ofs)
        {
            spdlog::error("保存博客失败：写入文件 {} 失败！", out_path.string());
            return std::string{ "保存博客失败" };
        }
        ofs << fm.str();
        ofs.close();
        return std::nullopt;
    }

    /**
     * @brief 删除博客 .md 文件及删除完成后为空的父目录。
     * @param file_path 博客文件相对路径。
     * @return std::nullopt 表示成功；否则返回错误消息。
     */
    auto delete_blog_md(std::string_view file_path) -> std::optional<std::string>
    {
        std::error_code ec;
        const std::filesystem::path blogs_root{ std::filesystem::path{ config::config()["FILE_PATH"] } / "blogs" };
        const std::filesystem::path md_path{ blogs_root / (std::string{ file_path } + ".md") };
        std::filesystem::remove(md_path, ec);
        if (ec)
        {
            spdlog::error("删除博客失败：{} - {}", md_path.string(), ec.message());
            return std::string{ "删除博客失败" };
        }

        // 父目录为空时清理目录
        const std::filesystem::path parent{ md_path.parent_path() };
        if (parent != blogs_root)
        {
            ec.clear();
            std::filesystem::remove(parent, ec);
            if (ec)
                spdlog::debug("保留博客父目录 {}：{}", parent.string(), ec.message());
        }
        return std::nullopt;
    }

} // namespace



namespace doc
{
    auto valid_blog_date(const std::string& update_time) -> std::string
    {
        if (is_valid_date(update_time))
            return update_time;
        return "";
    }

    auto get_categories(pqxx::connection& conn) -> std::vector<Category>
    {
        spdlog::debug("正在从数据库获取分类列表...");
        pqxx::work txn{ conn };

        // 仅返回 count > 0 的分类。
        const auto rows = txn.exec(
            "SELECT c.id, c.name, c.count FROM blog_categories c "
            "WHERE c.count > 0 "
            "ORDER BY c.id"
        );
        txn.commit();

        std::vector<Category> result;
        result.reserve(rows.size());
        for (const auto& row : rows)
        {
            result.push_back({
                row["id"]   .as<int>(),
                row["name"] .as<std::string>(),
                row["count"].as<int>()
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

        // 仅返回 count > 0 的标签。
        const std::string sql = category_ids.empty()
                              ? "SELECT t.id, t.name, t.count FROM blog_tags t "
                                "WHERE t.count > 0 "
                                "ORDER BY t.id"
                              : "SELECT t.id, t.name, t.count FROM blog_tags t "
                                "WHERE t.count > 0 AND EXISTS ("
                                "SELECT 1 FROM category_tag_relations ct "
                                "WHERE ct.tag_id = t.id "
                                "AND ct.category_id = ANY($1::int[]) AND ct.count > 0) "
                                "ORDER BY t.id";

        const auto rows = category_ids.empty()
                        ? txn.exec(sql)
                        : txn.exec(sql, pqxx::params{ "{" + join_ids(category_ids) + "}" });
        txn.commit();

        std::vector<Tag> result;
        result.reserve(rows.size());
        for (const auto& row : rows)
        {
            result.push_back({
                row["id"]   .as<int>(),
                row["name"] .as<std::string>(),
                row["count"].as<int>()
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

        auto filter = build_blog_filter(query);

        // 动态构建 SQL
        std::ostringstream sql;
        sql << "SELECT b.id, b.title, b.description, b.file_path, "
               "TO_CHAR(b.update_time, 'YYYY-MM-DD') AS update_time "
               "FROM blogs b "
            << filter.where;

        // 统一排序：更新日期晚的在前；更新日期相同则 id 大的在前
        sql << "ORDER BY b.update_time DESC, b.id DESC";

        // 按需分页：只取当前页的数据（page_size 为 0 时不分页，返回全部）
        if (query.page_size > 0)
        {
            const int page = query.page > 0 ? query.page : 1;
            sql << " LIMIT $" << (filter.param_count + 1)
                << " OFFSET $" << (filter.param_count + 2);
            filter.params.append(query.page_size);
            filter.params.append((page - 1) * query.page_size);
        }

        const auto rows = txn.exec(sql.str(), filter.params);

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

            result.push_back(std::move(item));
        }

        // 批量查询标签
        attach_tag_names(txn, blog_ids, result);

        // 批量查询分类
        attach_category_names(txn, blog_ids, result);

        txn.commit();
        spdlog::debug("从数据库获取博客列表完成。");
        return result;
    }

    auto count_blogs(
        pqxx::connection& conn,
        const BlogQuery&  query)
    -> int
    {
        spdlog::debug("正在统计符合条件的博客总数...");
        pqxx::work txn{ conn };

        const auto filter = build_blog_filter(query);

        // 与 get_blogs 相同的 FROM / 筛选条件，保证计数与列表口径一致
        std::ostringstream sql;
        sql << "SELECT COUNT(*) FROM blogs b " << filter.where;

        const auto row = txn.exec(sql.str(), filter.params);

        txn.commit();
        const int total = row[0][0].as<int>();
        spdlog::debug("符合条件的博客总数：{}", total);
        return total;
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
            "TO_CHAR(b.update_time, 'YYYY-MM-DD') AS update_time "
            "FROM blogs b "
            "WHERE b.file_path = $1",
            pqxx::params{ std::string{ file_path } }
        );

        if (row.empty())
        {
            spdlog::info("博客不存在：{}", file_path);
            return std::nullopt;
        }

        BlogItem item{};
        fill_blog_item(item, row[0]);

        // 查询分类与标签（items 拷贝 item，供下方日志继续使用 item.title）
        const std::vector<int> ids{ item.id };
        std::vector<BlogItem>  items{ item };

        attach_category_names(txn, ids, items);
        attach_tag_names(txn, ids, items);
        item = std::move(items.front());

        txn.commit();
        spdlog::debug("博客获取成功：{}", item.title);
        return item;
    }

    auto save_blog(
        pqxx::connection& conn,
        BlogContent&      blog,
        std::string_view  file_path_category,
        std::string_view  file_path_name)
    -> std::optional<std::string>
    {
        // 校验元信息字段
        if (auto err = validate_all_fields(blog.title, blog.description, blog.categories, blog.tags,
                                           file_path_category, file_path_name))
            return err;

        // 拼接博客文件的相对路径："category/name"
        const std::string file_path = std::string{ file_path_category } + "/" + std::string{ file_path_name };
        blog.file_path = file_path;

        pqxx::work txn{ conn };

        // 检查文件路径是否已被占用
        if (is_file_path_taken(txn, file_path))
            return "博客路径已存在";

        // 解析分类与标签：不存在的随即创建
        const auto category_ids = resolve_category_ids(txn, blog.categories);
        if (!category_ids)
            return "创建分类失败";

        const auto tag_ids = resolve_tag_ids(txn, blog.tags);
        if (!tag_ids)
            return "创建标签失败";

        // 插入博客数据库记录
        std::string dt{ blog.date };

        pqxx::result rows = txn.exec(
            "INSERT INTO blogs (title, description, content, file_path, update_time) "
            "VALUES ($1, $2, $3, $4, $5::date) RETURNING id",
            pqxx::params{ blog.title,
                          blog.description,
                          blog.content,
                          file_path, dt }
        );
        if (rows.empty())
        {
            spdlog::error("博客信息插入数据库失败。");
            return "创建博客记录失败";
        }

        const auto row = rows[0];
        const int blog_id = row["id"].as<int>();

        // 关联博客与分类、标签
        link_blog_categories(txn, blog_id, *category_ids);
        link_blog_tags(txn, blog_id, *tag_ids);
        // 为该博客的每个 (分类, 标签) 组合累加关联计数
        add_blog_category_tag_counts(txn, *category_ids, *tag_ids);
        // 更新分类 / 标签自身的计数
        refresh_category_tag_counts(txn, *category_ids, *tag_ids);

        txn.commit();

        // 生成 Frontmatter 并写入 .md
        if (auto err = write_blog_md(blog))
            return err;

        return std::nullopt;
    }

    auto delete_blog(
        pqxx::connection& conn,
        std::string_view  file_path)
    -> std::optional<std::string>
    {
        if (file_path.empty())
            return "缺少 file_path 参数";

        pqxx::work txn{ conn };

        // 查找博客记录
        auto blog = find_blog_row(txn, file_path);
        if (!blog)
            return "博客不存在";

        const int blog_id = *blog;

        // 记录该博客的分类和标签，用于删除后重新计数
        const auto category_ids = get_blog_category_ids(txn, blog_id);
        const auto tag_ids      = get_blog_tag_ids(txn, blog_id);
        // 删除博客记录（CASCADE 自动清理 blog_categories_relations / blog_tag_relations 关联）
        txn.exec("DELETE FROM blogs WHERE id = $1", pqxx::params{ blog_id });
        // 递减该博客涉及的 (分类, 标签) 关联计数，并重新计算各自计数
        remove_category_tag_counts(txn, category_ids, tag_ids);
        refresh_category_tag_counts(txn, category_ids, tag_ids);

        txn.commit();

        // 删除服务器上的 .md 文件及可能为空的父目录
        if (auto err = delete_blog_md(file_path))
            return err;

        return std::nullopt;
    }

    auto update_blog(
        pqxx::connection& conn,
        BlogContent&      blog,
        std::string_view  old_file_path,
        std::string_view  file_path_category,
        std::string_view  file_path_name)
    -> std::optional<std::string>
    {
        // 校验元信息字段
        if (auto err = validate_all_fields(blog.title, blog.description, blog.categories, blog.tags,
                                           file_path_category, file_path_name))
            return err;

        const std::string file_path = std::string{ file_path_category } + "/" + std::string{ file_path_name };
        blog.file_path = file_path;

        const bool path_changed = (old_file_path != blog.file_path);

        pqxx::work txn{ conn };

        // 查找旧博客记录（blog_opt 为可选的博客 ID，避免与参数 blog 同名）
        auto blog_opt = find_blog_row(txn, old_file_path);
        if (!blog_opt)
            return "旧博客不存在";

        const int blog_id = *blog_opt;

        // 更新前记录旧的分类 / 标签，用于递减计数
        const auto old_category_ids = get_blog_category_ids(txn, blog_id);
        const auto old_tag_ids      = get_blog_tag_ids(txn, blog_id);

        // 解析新分类与新标签：不存在的随即创建
        const auto new_category_ids = resolve_category_ids(txn, blog.categories);
        if (!new_category_ids)
        {
            spdlog::error("更新博客失败：解析分类失败。");
            return "创建分类失败";
        }

        const auto new_tag_ids = resolve_tag_ids(txn, blog.tags);
        if (!new_tag_ids)
        {
            spdlog::error("更新博客失败：解析标签失败。");
            return "创建标签失败";
        }

        // 路径变更时检查新路径是否已被占用
        if (path_changed && is_file_path_taken(txn, file_path))
        {
            spdlog::error("更新博客失败：新路径 {} 已被占用。", file_path);
            return "新文件路径已被其他博客占用";
        }

        // 更新博客数据库记录
        std::string dt{ blog.date };
        txn.exec(
            "UPDATE blogs SET title = $1, description = $2, content = $3, "
            "update_time = $4::date, file_path = $5 WHERE id = $6",
            pqxx::params{ blog.title, blog.description,
                          blog.content, dt,
                          file_path, blog_id });

        // 重建博客-分类关联（先删后插）
        txn.exec("DELETE FROM blog_categories_relations WHERE blog_id = $1", pqxx::params{ blog_id });
        link_blog_categories(txn, blog_id, *new_category_ids);

        // 重建博客-标签关联（先删后插）
        txn.exec("DELETE FROM blog_tag_relations WHERE blog_id = $1", pqxx::params{ blog_id });
        link_blog_tags(txn, blog_id, *new_tag_ids);

        // 撤下旧组合的计数，再为新组合建立关联并累加计数
        // 分类 / 标签本身不删除：无博客使用时保留在库中，只是不再出现在筛选器里
        remove_category_tag_counts(txn, old_category_ids, old_tag_ids);
        add_blog_category_tag_counts(txn, *new_category_ids, *new_tag_ids);

        // 刷新新旧分类 / 标签的博客计数（新旧 ID 合并后一次重算）
        auto refresh_category_ids = old_category_ids;
        refresh_category_ids.insert(refresh_category_ids.end(), new_category_ids->begin(), new_category_ids->end());

        auto refresh_tag_ids = old_tag_ids;
        refresh_tag_ids.insert(refresh_tag_ids.end(), new_tag_ids->begin(), new_tag_ids->end());

        refresh_category_tag_counts(txn, refresh_category_ids, refresh_tag_ids);

        txn.commit();

        // 如果文件路径变更，删除旧 .md 文件
        if (path_changed)
        {
            if (auto err = delete_blog_md(old_file_path); err)
                return err;
        }

        // 生成 Frontmatter 并写入新 .md 文件
        if (auto err = write_blog_md(blog); err)
            return err;

        spdlog::info("博客更新成功。");
        return std::nullopt;
    }

} // namespace doc
