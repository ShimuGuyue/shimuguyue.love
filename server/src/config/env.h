/**
 * @file config/env.h
 * @brief 环境变量获取、初始化与存储
 */
#pragma once

#include <string>

#include "config/env_map.h"

namespace config
{
    /**
     * @brief 读取并存储全部必需环境变量。
     *        未设置或无效则打印错误并调用 std::exit(1)。
     */
    void init();

    /**
     * @brief 读取必需的环境变量，未设置时打印错误信息并退出程序。
     * @param key 环境变量名。
     * @return 环境变量的值。
     */
    [[nodiscard]]auto get_env(const char* key) -> std::string;

    /**
     * @brief 获取环境变量单例存储。
     * @return 环境变量存储的只读引用。
     */
    [[nodiscard]] auto env() -> const EnvMap&;

}
