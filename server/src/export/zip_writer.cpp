/**
 * @file export/zip_writer.cpp
 * @brief zip 打包工具实现（store 方式，无压缩）
 */

#include "export/zip_writer.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
    /**
     * @brief 生成 CRC-32 查找表（首次调用时初始化）。
     * @return CRC-32 查找表。
     */
    auto crc32_table() -> const std::array<uint32_t, 256>&
    {
        static const std::array<uint32_t, 256> table = []
        {
            std::array<uint32_t, 256> t{ };
            for (uint32_t i{ 0 }; i < 256; ++i)
            {
                uint32_t c{ i };
                for (int k{ 0 }; k < 8; ++k)
                {
                    c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                t[i] = c;
            }
            return t;
        }();
        return table;
    }

    /**
     * @brief 计算字符串的 CRC-32 校验值。
     * @param data 输入数据。
     * @return CRC-32 值。
     */
    auto crc32(const std::string& data) -> uint32_t
    {
        uint32_t crc = 0xFFFFFFFFu;
        for (const unsigned char c : data)
        {
            crc = crc32_table()[(crc ^ c) & 0xFFu] ^ (crc >> 8);
        }
        return crc ^ 0xFFFFFFFFu;
    }

    /**
     * @brief 以小端序追加 uint16。
     * @param out   输出字节流。
     * @param value 待写入值。
     */
    auto append_u16(std::string& out, uint16_t value) -> void
    {
        out.push_back(static_cast<char>( value       & 0xFFu));
        out.push_back(static_cast<char>((value >> 8) & 0xFFu));
    }

    /**
     * @brief 以小端序追加 uint32。
     * @param out   输出字节流。
     * @param value 待写入值。
     */
    auto append_u32(std::string& out, uint32_t value) -> void
    {
        out.push_back(static_cast<char>( value        & 0xFFu));
        out.push_back(static_cast<char>((value >> 8)  & 0xFFu));
        out.push_back(static_cast<char>((value >> 16) & 0xFFu));
        out.push_back(static_cast<char>((value >> 24) & 0xFFu));
    }

} // namespace






namespace zip_writer
{
    auto build_zip(const std::vector<std::pair<std::string, std::string>>& files) -> std::string
    {
        std::string out;
        out.reserve(1024 + files.size() * 64);
        std::vector<std::string> central;
        central.reserve(files.size());
        uint32_t local_offset{ 0 };

        for (const auto& [name, data] : files)
        {
            const auto crc  = crc32(data);
            const auto size = static_cast<uint32_t>(data.size());

            // 本地文件头（store 方式）
            out.append("PK\x03\x04", 4);
            append_u16(out, 20);                                  // 解压所需最低版本号
            append_u16(out, 0);                                   // 通用标志位
            append_u16(out, 0);                                   // 压缩方式：0 = 不压缩（store）
            append_u16(out, 0);                                   // 修改时间
            append_u16(out, 0x21);                                // 修改日期：1980-01-01
            append_u32(out, crc);                                 // CRC-32 校验值
            append_u32(out, size);                                // 压缩后大小
            append_u32(out, size);                                // 未压缩大小
            append_u16(out, static_cast<uint16_t>(name.size()));  // 文件名长度
            append_u16(out, 0);                                   // 扩展字段长度
            out += name;
            out += data;

            // 中央目录条目
            std::string entry;
            entry.append("PK\x01\x02", 4);
            append_u16(entry, 20);                                  // 生成此条目的版本号
            append_u16(entry, 20);                                  // 解压所需最低版本号
            append_u16(entry, 0);                                   // 通用标志位
            append_u16(entry, 0);                                   // 压缩方式：0 = 不压缩（store）
            append_u16(entry, 0);                                   // 修改时间
            append_u16(entry, 0x21);                                // 修改日期：1980-01-01
            append_u32(entry, crc);                                 // CRC-32 校验值
            append_u32(entry, size);                                // 压缩后大小
            append_u32(entry, size);                                // 未压缩大小
            append_u16(entry, static_cast<uint16_t>(name.size()));  // 文件名长度
            append_u16(entry, 0);                                   // 扩展字段长度
            append_u16(entry, 0);                                   // 注释长度
            append_u16(entry, 0);                                   // 起始磁盘编号
            append_u16(entry, 0);                                   // 内部文件属性
            append_u32(entry, 0);                                   // 外部文件属性
            append_u32(entry, local_offset);                        // 本地文件头在压缩包中的偏移
            entry += name;
            central.push_back(std::move(entry));

            local_offset += 30 + static_cast<uint32_t>(name.size()) + size;
        }

        const auto central_offset = static_cast<uint32_t>(out.size());
        uint32_t central_size{ 0 };
        for (const auto& entry : central)
        {
            out += entry;
            central_size += static_cast<uint32_t>(entry.size());
        }

        // 结束记录（EOCD）
        out.append("PK\x05\x06", 4);
        append_u16(out, 0);                                    // 当前磁盘编号
        append_u16(out, 0);                                    // 中央目录所在磁盘编号
        append_u16(out, static_cast<uint16_t>(files.size()));  // 当前磁盘上的条目数
        append_u16(out, static_cast<uint16_t>(files.size()));  // 中央目录总条目数
        append_u32(out, central_size);                         // 中央目录总大小
        append_u32(out, central_offset);                       // 中央目录起始偏移
        append_u16(out, 0);                                    // 注释长度
        return out;
    }

} // namespace zip_writer
