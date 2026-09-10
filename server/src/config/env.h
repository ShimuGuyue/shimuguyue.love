/**
 * @file config/env.h
 * @brief 环境变量（conf/.env）加载与初始化
 */
#pragma once

namespace config
{
    /**
     * @brief 从 conf/.env 文件读取并存储全部必需环境变量。
     *        未设置或无效则打印错误并调用 std::exit(1)。
     */
    void init_env();

}
