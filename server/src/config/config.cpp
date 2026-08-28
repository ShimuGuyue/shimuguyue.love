/**
 * @file config/config.cpp
 * @brief 配置统一初始化入口实现
 */

#include "config/config.h"

#include "config/env.h"

namespace config
{
    void init()
    {
        init_env();
    }

} // namespace config
