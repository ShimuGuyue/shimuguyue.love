/**
 * @file config/env.h
 * @brief 系统环境变量获取函数的定义
 */
#pragma once

#include <string>

namespace config
{

/**
 * @brief 读取必需的环境变量，未设置时打印错误信息并退出程序。
 * @param key 环境变量名。
 * @return 环境变量的值。
 */
[[nodiscard]]auto get_env(const char* key) -> std::string;

}