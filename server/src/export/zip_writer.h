/**
 * @file export/zip_writer.h
 * @brief zip 打包工具（store 方式，无压缩）
 */
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace zip_writer
{
    /**
     * @brief 将若干文件打包为 zip（store 方式，无压缩）。
     * @param files 文件名与内容对列表。
     * @return zip 二进制内容。
     */
    [[nodiscard]] auto build_zip(const std::vector<std::pair<std::string, std::string>>& files) -> std::string;

} // namespace zip_writer
