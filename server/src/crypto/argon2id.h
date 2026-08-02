/**
 * @file crypto/argon2id.h
 * @brief Argon2id 密码哈希类 — 基于 libsodium
 */
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace crypto
{

/**
 * @brief Argon2id 密码哈希工具类。
 *
 * 随机盐哈希使用 libsodium 的 crypto_pwhash_str / crypto_pwhash_str_verify
 * 高级 API，自动生成随机盐，返回 $argon2id$... 格式的编码哈希字符串。
 * 
 * 固定盐哈希使用固定盐值（crypto_pwhash 低级 API），输出 hex 编码字符串，
 * 安全参数使用 SENSITIVE 级别（~1 GB 内存，~2 秒耗时）。
 */
class Argon2id
{
public:
    Argon2id() = delete;

    /**
     * @brief 对明文数据进行 Argon2id 哈希（随机盐）。
     * @param data 明文数据。
     * @return 成功返回编码后的哈希字符串（含算法标识、参数、盐和哈希值），
     *         失败返回 std::nullopt。
     */
    [[nodiscard]] static auto hash_with_random_salt(std::string_view data) -> std::optional<std::string>;

    /**
     * @brief 对明文数据进行 Argon2id 哈希（固定盐）。
     *
     * 盐值为编译期硬编码常量，同一 key 永远产生相同哈希值。
     *
     * @param data 明文数据。
     * @return 成功返回编码后的哈希字符串（含算法标识、参数、盐和哈希值），
     *         失败返回 std::nullopt。
     */
    [[nodiscard]] static auto hash_with_fixed_salt(std::string_view data) -> std::optional<std::string>;

    /**
     * @brief 验证明文数据是否与哈希值匹配。
     * @param data 待验证的明文数据。
     * @param hash 明文数据对应的哈希值。
     * @return 匹配明文密码与哈希值匹配结果。
     */
    [[nodiscard]] static auto verify(std::string_view data, std::string_view hash) -> bool;
};

} // namespace crypto
