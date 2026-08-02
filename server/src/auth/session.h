/**
 * @file auth/session.h
 * @brief 基于数据库的 token 会话管理
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <pqxx/pqxx>

namespace auth
{

/// 会话信息
struct SessionInfo
{
    int                        user_id;
    std::vector<std::string>   permissions;
};

/**
 * @brief 创建会话 token 并写入数据库。
 * @param conn        数据库连接。
 * @param user_id     用户 ID。
 * @param permissions 用户权限列表。
 * @return 64 字符十六进制 token。
 */
[[nodiscard]] auto create_session(
    pqxx::connection&               conn,
    int                             user_id,
    const std::vector<std::string>& permissions)
-> std::string;

/**
 * @brief 验证 token 并从数据库获取会话信息。
 * @param conn  数据库连接。
 * @param token 会话 token。
 * @return 有效返回 SessionInfo，无效或过期返回 std::nullopt。
 */
[[nodiscard]] auto validate_session(
    pqxx::connection& conn,
    std::string_view  token)
-> std::optional<SessionInfo>;

/**
 * @brief 删除过期会话
 * @param conn 数据库连接。
 */
void cleanup_expired_sessions(pqxx::connection& conn);

} // namespace auth
