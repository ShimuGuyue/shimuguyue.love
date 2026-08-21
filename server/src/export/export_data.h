/**
 * @file export/export_data.h
 * @brief 后台数据导出：将博客/用户数据表分别导出为 zip 压缩包
 */
#pragma once

#include <expected>
#include <string>

#include <pqxx/pqxx>

namespace export_data
{
    /**
     * @brief 导出博客相关数据表为 zip 压缩包（blogs / categories / tags / blog_tags），
     *        并附加服务器博客目录（FILE_PATH/doc/blogs）下的全部文件。
     * @param conn 数据库连接。
     * @return 成功返回 zip 二进制内容；失败返回错误消息。
     */
    [[nodiscard]] auto build_blogs_export_zip(pqxx::connection& conn) -> std::expected<std::string, std::string>;

    /**
     * @brief 导出用户相关数据表为 zip 压缩包（users / permissions / user_permissions）。
     * @param conn 数据库连接。
     * @return 成功返回 zip 二进制内容；失败返回错误消息。
     */
    [[nodiscard]] auto build_users_export_zip(pqxx::connection& conn) -> std::expected<std::string, std::string>;

} // namespace export_data
