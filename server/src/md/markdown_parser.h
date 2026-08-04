/**
 * @file md/markdown_parser.h
 * @brief Markdown 文件解析：提取 YAML frontmatter 与正文
 */
#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace md
{
    /**
     * @brief 解析 markdown 文本中的 YAML frontmatter。
     *
     * @param raw  原始 markdown 文本。
     * @return     JSON 对象 { title, description, category, tags, file_path, content }。
     */
    [[nodiscard]] auto parse_frontmatter(const std::string& raw) -> nlohmann::json;

} // namespace md
