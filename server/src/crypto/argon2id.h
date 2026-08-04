/**
 * @file crypto/argon2id.h
 * @brief Argon2id 密码哈希工具 — 基于 libsodium
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace crypto
{
/**
 * @brief 哈希验证结果。
 */
enum class VerifyResult
{
    Match,       ///< 验证通过
    Mismatch,    ///< 数据与哈希不匹配
    SystemError, ///< 系统错误
};

} // namespace crypto





/**
 * @brief Argon2id 密码哈希工具命名空间。
 *
 * 随机盐哈希使用 libsodium 的 crypto_pwhash_str / crypto_pwhash_str_verify
 * 高级 API，自动生成随机盐，返回 $argon2id$... 格式的编码哈希字符串。
 *
 * 固定盐哈希使用固定盐值（crypto_pwhash 低级 API），输出 hex 编码字符串，
 * 安全参数使用 SENSITIVE 级别（~1 GB 内存，~2 秒耗时）。
 */
namespace crypto::Argon2id
{
    /**
     * @brief 对明文数据进行随机盐哈希。
     * @param data 明文数据。
     * @return 成功返回编码后的哈希字符串，
     *         失败返回 std::nullopt。
     */
    [[nodiscard]] auto hash_with_random_salt(std::string_view data) -> std::optional<std::string>;

    /**
     * @brief 对明文数据进行固定盐哈希。
     *
     * 盐值为编译期硬编码常量，同一 key 永远产生相同哈希值。
     *
     * @param data 明文数据。
     * @return 成功返回编码后的哈希字符串，
     *         失败返回 std::nullopt。
     */
    [[nodiscard]] auto hash_with_fixed_salt(std::string_view data) -> std::optional<std::string>;

    /**
     * @brief 使用随机盐哈希格式验证明文数据是否与哈希值匹配。
     * @param data 待验证的明文数据。
     * @param hash 随机盐哈希字符串（$argon2id$...）。
     * @return VerifyResult。
    */
    [[nodiscard]] auto verify_with_random_salt(std::string_view data, std::string_view hash) -> VerifyResult;

} // namespace crypto::Argon2id
