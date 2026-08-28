/**
 * @file config/config.h
 * @brief 配置统一初始化入口
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace config
{
    /**
     * @brief 从当前目录向上查找项目 conf/ 目录下的配置文件。
     * @param filename 配置文件名，例如 ".env" 或 "cache.yml"。
     * @return 完整路径；未找到返回 std::nullopt。
     */
    [[nodiscard]] auto find_config_file(const std::string& filename)
        -> std::optional<std::filesystem::path>;

    /**
     * @brief 统一初始化全部配置。
     *
     * 依次调用 init_env()（conf/.env）与 init_cache()（conf/cache.yml）；
     * 任一配置缺失或非法都会打印错误并 exit(1)。
     */
    void init();

} // namespace config
