/**
 * @file crypto/argon2id.cpp
 * @brief Argon2id 密码哈希实现
 */

#include "crypto/argon2id.h"

#include "config/env.h"

#include <array>

#include <sodium.h>
#include <spdlog/spdlog.h>

namespace
{
/**
 * @brief 将二进制数据转换为 hex 字符串。
 * @param data 二进制数据指针。
 * @param len  数据长度。
 * @return hex 编码字符串。
 */
auto bin_to_hex(const unsigned char* data, std::size_t len) -> std::string
{
    static constexpr char hex_chars[] = "0123456789abcdef";
    std::string result(len * 2, '\0');
    for (std::size_t i{ 0 }; i < len; ++i)
    {
        result[i * 2]     = hex_chars[(data[i] >> 4) & 0x0f];
        result[i * 2 + 1] = hex_chars[ data[i]       & 0x0f];
    }
    return result;
}

/**
 * @brief 将 hex 字符转换为半字节值。
 * @param c hex 字符。
 * @return 0~15；无效字符返回 -1。
 */
auto hex_char_to_nibble(char c) -> int
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * @brief 将 hex 字符串解码为盐值字节数组。
 * @param hex  hex 编码字符串（长度须为盐值字节数的两倍）。
 * @param salt 输出盐值字节数组。
 * @return 解码成功返回 true，失败返回 false。
 */
auto hex_to_salt(std::string_view hex, std::array<unsigned char, crypto_pwhash_SALTBYTES>& salt) -> bool
{
    if (hex.size() != salt.size() * 2)
        return false;

    for (std::size_t i{ 0 }; i < salt.size(); ++i)
    {
        const auto hi = hex_char_to_nibble(hex[i * 2]);
        const auto lo = hex_char_to_nibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        salt[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}

} // namespace





namespace crypto::Argon2id
{
    auto hash_with_random_salt(std::string_view data) -> std::optional<std::string>
    {
        spdlog::debug("正在进行 Argon2id 随机盐哈希...");
        if (data.empty())
        {
            spdlog::debug("随机盐哈希失败：输入数据为空。");
            return std::nullopt;
        }

        // 使用 libsodium 的 crypto_pwhash_str 生成随机盐哈希字符串
        // 输出格式：$argon2id$v=19$m=...,t=...,p=...$<salt>$<hash>
        std::string res(crypto_pwhash_STRBYTES, '\0');
        const auto result = static_cast<int>(
            crypto_pwhash_str(
                res.data(),
                data.data(),
                data.size(),
                crypto_pwhash_OPSLIMIT_SENSITIVE,
                crypto_pwhash_MEMLIMIT_SENSITIVE
            )
        );

        if (result != 0)
        {
            spdlog::error("随机盐哈希失败：crypto_pwhash_str 返回错误码 {}", result);
            return std::nullopt;
        }

        // 去除 crypto_pwhash_STRBYTES 尾部包含的 '\0'，返回紧凑字符串
        res.resize(std::char_traits<char>::length(res.data()));
        spdlog::debug("随机盐哈希完成。");
        return res;
    }

    auto hash_with_fixed_salt(std::string_view data) -> std::optional<std::string>
    {
        spdlog::debug("正在进行 Argon2id 固定盐哈希...");
        if (data.empty())
        {
            spdlog::debug("固定盐哈希失败：输入数据为空。");
            return std::nullopt;
        }

        // 使用环境变量 FIXED_SALT 指定的盐值进行确定性哈希，输出为 32 字节二进制
        // crypto_pwhash_BYTES = 32（libsodium 固定值；unofficial-sodium 未导出该宏）
        std::array<unsigned char, crypto_pwhash_SALTBYTES> salt{ };
        const auto salt_hex = config::env()["FIXED_SALT"];
        if (!hex_to_salt(salt_hex, salt))
        {
            spdlog::error("固定盐哈希失败：FIXED_SALT 格式无效。");
            return std::nullopt;
        }

        std::array<unsigned char, 32> hash_out{ };

        const auto result = static_cast<int>(
            crypto_pwhash(
                hash_out.data(),
                hash_out.size(),
                data.data(),
                data.size(),
                salt.data(),
                crypto_pwhash_OPSLIMIT_SENSITIVE,
                crypto_pwhash_MEMLIMIT_SENSITIVE,
                crypto_pwhash_ALG_DEFAULT
            )
        );

        if (result != 0)
        {
            spdlog::error("固定盐哈希失败：crypto_pwhash 返回错误码 {}", result);
            return std::nullopt;
        }

        // 将二进制哈希输出转为 hex 字符串便于存储
        auto hex = bin_to_hex(hash_out.data(), hash_out.size());
        spdlog::debug("固定盐哈希完成：{}", hex);
        return hex;
    }

    auto verify_with_random_salt(std::string_view data, std::string_view hash) -> VerifyResult
    {
        spdlog::debug("正在进行 Argon2id 随机盐验证...");
        if (hash.empty() || data.empty())
        {
            spdlog::debug("Argon2id 随机盐验证失败：输入数据或哈希值为空。");
            return VerifyResult::Mismatch;
        }

        // 使用 libsodium 的 crypto_pwhash_str_verify 验证密码与哈希是否匹配
        // 返回值：0 = 匹配成功；-1 = 密码不匹配（或哈希格式无效）；-2 = 系统错误
        const auto result = static_cast<int>(
            crypto_pwhash_str_verify(
                hash.data(),
                data.data(),
                data.size()
            )
        );

        if (result == 0)
        {
            spdlog::debug("Argon2id 随机盐验证通过。");
            return VerifyResult::Match;
        }

        if (result == -1)
        {
            spdlog::debug("Argon2id 随机盐验证失败：数据与哈希不匹配。");
            return VerifyResult::Mismatch;
        }

        // 系统错误
        spdlog::error("Argon2id 随机盐验证发生系统错误：crypto_pwhash_str_verify 返回错误码 {}", result);
        return VerifyResult::SystemError;
    }

} // namespace crypto::Argon2id
