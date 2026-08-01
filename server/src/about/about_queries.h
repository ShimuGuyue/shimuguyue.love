/**
 * @file about/about_queries.h
 * @brief 《关于我》页面数据库查询
 */
#pragma once

#include <pqxx/pqxx>

#include <string>

namespace about
{

/**
 * @brief 获取《关于我》页面的 README 内容。
 * @param conn 数据库连接。
 * @return README.md 的 Markdown 原文。
 */
[[nodiscard]] auto get_about(pqxx::connection& conn) -> std::string;

} // namespace about
