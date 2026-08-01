/**
 * @file md/markdown_parser.cpp
 * @brief 使用 yaml-cpp 解析 Markdown YAML frontmatter
 */

#include "md/markdown_parser.h"

#include "config/env.h"

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace md
{

static std::string doc_path_;

void init()
{
    doc_path_ = config::get_env("DOC_PATH");
}

auto doc_path() -> const std::string&
{
    return doc_path_;
}

auto parse_frontmatter(const std::string& raw) -> nlohmann::json
{
    std::string text = raw;

    // 去除 UTF-8 BOM
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF)
        text.erase(0, 3);

    // CRLF → LF
    for (size_t pos = text.find("\r\n"); pos != std::string::npos; pos = text.find("\r\n", pos))
        text.replace(pos, 2, "\n");

    nlohmann::json result;
    result["title"] = "";
    result["description"] = "";
    result["category"] = "";
    result["tags"] = nlohmann::json::array();
    result["file_path"] = "";
    result["file_path_category"] = "";
    result["file_path_name"] = "";
    result["content"] = text;


    // 查找 frontmatter 分隔符
    auto first_delim = text.find("---\n");
    if (first_delim == std::string::npos)
        return result;

    auto second_delim = text.find("\n---\n", first_delim + 4);
    if (second_delim == std::string::npos)
    {
        second_delim = text.find("\n---", first_delim + 4);
        if (second_delim == std::string::npos)
            return result;
    }

    std::string fm_text = text.substr(first_delim + 4, second_delim - first_delim - 4);
    std::string content = text.substr(second_delim + 4);
    while (!content.empty() && content[0] == '\n') content.erase(0, 1);

    result["content"] = content;

    // 删除 YAML 解析会导致异常的空值行
    std::regex empty_line(R"(^\w+:\s*$)", std::regex::multiline);
    fm_text = std::regex_replace(fm_text, empty_line, "");

    // 提取所需元数据
    YAML::Node fm = YAML::Load(fm_text);

    if (fm["title"])
        result["title"] = fm["title"].as<std::string>();

    if (fm["description"])
        result["description"] = fm["description"].as<std::string>();

    if (fm["category"])
        result["category"] = fm["category"].as<std::string>();

    if (fm["tags"] && fm["tags"].IsSequence())
    {
        for (const auto& t : fm["tags"])
        {
            result["tags"].push_back(t.as<std::string>());
        }
    }

    if (fm["file_path"])
    {
        std::string fp = fm["file_path"].as<std::string>();
        result["file_path"] = fp;
        auto slash = fp.find('/');
        if (slash != std::string::npos)
        {
            result["file_path_category"] = fp.substr(0, slash);
            result["file_path_name"] = fp.substr(slash + 1);
        }
        else
        {
            result["file_path_category"] = "";
            result["file_path_name"] = fp;
        }
    }

    return result;
}

} // namespace md
