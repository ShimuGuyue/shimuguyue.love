/**
 * @file config/env.h
 * @brief 环境变量加载、初始化与存储
 */
#pragma once

#include <string>

#include "config/env_map.h"

namespace config
{
    /**
     * @brief 从 .env 文件读取并存储全部必需环境变量。
     *        未设置或无效则打印错误并调用 std::exit(1)。
     */
    void init_env();

    /**
     * @brief 获取环境变量单例存储。
     * @return 环境变量存储的只读引用。
     */
    [[nodiscard]] auto env() -> const EnvMap&;

}
