/**
 * @file crypto/argon2id.cpp
 * @brief Argon2id 密码哈希实现
 */

#include "crypto/argon2id.h"

#include <array>
#include <sodium.h>
#include <spdlog/spdlog.h>

namespace
{

/// 固定盐哈希专用固定盐值（16 字节编译期常量）。
constexpr std::array<unsigned char, crypto_pwhash_SALTBYTES> salt_fixed = {
    0x71, 0x68, 0xf8, 0x3d, 0x9a, 0x4e, 0xb5, 0x2c,
    0x15, 0x7a, 0x6d, 0xe1, 0x93, 0x0f, 0x42, 0x88
};

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
    for (std::size_t i = 0; i < len; ++i)
    {
        result[i * 2]     = hex_chars[(data[i] >> 4) & 0x0f];
        result[i * 2 + 1] = hex_chars[ data[i]       & 0x0f];
    }
    return result;
}

} // namespace



namespace crypto
{

auto Argon2id::hash_with_random_salt(std::string_view data) -> std::optional<std::string>
{
    spdlog::info("正在进行 Argon2id 随机盐哈希...");
    if (data.empty())
    {
        spdlog::info("随机盐哈希失败：输入数据为空。");
        return std::nullopt;
    }

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
    spdlog::info("随机盐哈希完成。");
    return res;
}

auto Argon2id::hash_with_fixed_salt(std::string_view data) -> std::optional<std::string>
{
    spdlog::info("正在进行 Argon2id 固定盐哈希...");
    if (data.empty())
    {
        spdlog::info("固定盐哈希失败：输入数据为空。");
        return std::nullopt;
    }

    // crypto_pwhash_BYTES = 32（libsodium 固定值；unofficial-sodium 未导出该宏）
    std::array<unsigned char, 32> hash_out{};

    const auto result = static_cast<int>(
        crypto_pwhash(
            hash_out.data(),
            hash_out.size(),
            data.data(),
            data.size(),
            salt_fixed.data(),
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

    auto hex = bin_to_hex(hash_out.data(), hash_out.size());
    spdlog::info("固定盐哈希完成：{}", hex);
    return hex;
}

auto Argon2id::verify(std::string_view data, std::string_view hash) -> bool
{
    spdlog::info("正在进行 Argon2id 验证...");
    if (hash.empty() || data.empty())
    {
        spdlog::info("Argon2id 验证失败：输入数据或哈希值为空。");
        return false;
    }

    auto result = static_cast<int>(
        crypto_pwhash_str_verify(
            hash.data(),
            data.data(),
            data.size()
        )
    );

    if (result != 0)
    {
        spdlog::info("Argon2id 验证失败：密码不匹配。");
        return false;
    }
    spdlog::info("Argon2id 验证通过。");
    return true;
}

} // namespace crypto
