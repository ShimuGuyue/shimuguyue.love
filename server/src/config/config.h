/**
 * @file config/config.h
 * @brief 配置统一初始化入口
 */
#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "config/config_map.h"

namespace config
{
    /**
     * @brief 获取配置项单例存储（.env 环境变量与 cache.yml / page_size.yml 配置项）。
     * @return 配置项存储的只读引用。
     */
    [[nodiscard]] auto config() -> const ConfigMap&;

    /**
     * @brief 从当前目录向上查找项目 conf/ 配置目录。
     *
     * 约定所有配置文件都放在 conf/ 目录下，因此只需统一找到该目录：
     * 目录地址在进程内只查找一次，各配置模块（.env / cache.yml /
     * page_size.yml）再在目录内取各自的文件。
     *
     * @return conf/ 目录的完整路径；未找到返回 std::nullopt。
     */
    [[nodiscard]] auto find_conf_dir()
        -> std::optional<std::filesystem::path>;

    /**
     * @brief 定位 conf/ 目录内的配置文件。
     *
     * 经 find_conf_dir() 统一找到 conf/ 目录后拼接文件名。
     *
     * @param filename conf/ 目录内的配置文件名，例如 ".env" 或 "cache.yml"。
     * @return 配置文件完整路径；目录或文件缺失返回 std::nullopt。
     */
    [[nodiscard]] auto find_config_file(const std::string& filename)
        -> std::optional<std::filesystem::path>;

    /**
     * @brief 统一初始化全部配置。
     *
     * 依次调用 init_env()（conf/.env）、init_cache()（conf/cache.yml）
     * 与 init_page_size()（conf/page_size.yml）；
     * 任一配置缺失或非法都会打印错误并 exit(1)。
     */
    void init();

} // namespace config
