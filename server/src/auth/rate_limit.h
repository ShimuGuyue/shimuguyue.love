/**
 * @file auth/rate_limit.h
 * @brief 登录频率限制
 */
#pragma once

#include <string>

namespace auth
{

/**
 * @brief 检查指定 IP 是否超出登录频率限制。
 *
 * 在冷却时间窗口内，失败次数达到上限则拒绝登录。
 * 同时清理已过期的历史记录。
 *
 * @param ip 客户端 IP 地址。
 * @return true  已被限制，应拒绝登录。
 * @return false 未超限，可以尝试登录。
 */
[[nodiscard]] auto is_rate_limited(const std::string& ip) -> bool;

/**
 * @brief 记录一次登录失败。
 * @param ip 客户端 IP 地址。
 */
void record_failure(const std::string& ip);

/**
 * @brief 登录成功后清除该 IP 的失败记录。
 * @param ip 客户端 IP 地址。
 */
void clear(const std::string& ip);

} // namespace auth
