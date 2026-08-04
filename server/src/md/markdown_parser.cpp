/**
 * @file md/markdown_parser.cpp
 * @brief 使用 yaml-cpp 解析 Markdown YAML frontmatter
 */

#include "md/markdown_parser.h"

#include <regex>
#include <string>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace md
{
    auto parse_frontmatter(const std::string& raw) -> nlohmann::json
    {
        spdlog::debug("正在解析 md 文本的 fromtmatter 信息...");
        std::string text = raw;

        // 去除 UTF-8 BOM（Byte Order Mark, EF BB BF）
        // Windows 记事本等编辑器可能在文件头部插入 BOM，需先清除才能正确解析
        if (text.size() >= 3
            && static_cast<unsigned char>(text[0]) == 0xEF
            && static_cast<unsigned char>(text[1]) == 0xBB
            && static_cast<unsigned char>(text[2]) == 0xBF)
            text.erase(0, 3);

        // CRLF → LF：统一换行符
        for (size_t pos = text.find("\r\n"); pos != std::string::npos; pos = text.find("\r\n", pos))
        {
            text.replace(pos, 2, "\n");
        }

        nlohmann::json json;
        json["title"]              = "";
        json["description"]        = "";
        json["category"]           = "";
        json["tags"]               = nlohmann::json::array();
        json["file_path_category"] = "";
        json["file_path_name"]     = "";
        json["content"]            = text;


        // 查找 YAML frontmatter 分隔符 "---"
        // frontmatter 位于文件最开头，以 "---\n" 开始，以 "\n---\n" 或 "\n---" 结束
        auto first_delim = text.find("---\n");
        if (first_delim == std::string::npos)
            return json;
        auto second_delim = text.find("\n---\n", first_delim + 4);
        if (second_delim == std::string::npos)
        {
            second_delim = text.find("\n---", first_delim + 4);
            if (second_delim == std::string::npos)
                return json;
        }

        std::string fm_text = text.substr(first_delim + 4, second_delim - first_delim - 4);
        std::string content = text.substr(second_delim + 4);
        // 去除 frontmatter 与正文间空白行
        while (!content.empty() && content[0] == '\n')
        {
            content.erase(0, 1);
        }

        json["content"] = content;

        // 删除 YAML 解析会导致异常的空值行
        std::regex empty_line(R"(^\w+:\s*$)", std::regex::multiline);
        fm_text = std::regex_replace(fm_text, empty_line, "");

        // 提取所需元数据
        YAML::Node fm = YAML::Load(fm_text);

        if (fm["title"])
            json["title"]       = fm["title"]      .as<std::string>();
        if (fm["description"])
            json["description"] = fm["description"].as<std::string>();
        if (fm["category"])
            json["category"]    = fm["category"]   .as<std::string>();
        if (fm["tags"] && fm["tags"].IsSequence())
        {
            for (const auto& t : fm["tags"])
            {
                json["tags"].push_back(t.as<std::string>());
            }
        }
        if (fm["file_path"])
        {
            std::string fp = fm["file_path"].as<std::string>();
            auto slash = fp.find('/');
            if (slash != std::string::npos)
            {
                json["file_path_category"] = fp.substr(0, slash);
                json["file_path_name"] = fp.substr(slash + 1);
            }
            else
            {
                json["file_path_category"] = "";
                json["file_path_name"] = fp;
            }
        }

        spdlog::info("md 文本的 fromtmatter 信息解析完成。");
        return json;
    }

} // namespace md
