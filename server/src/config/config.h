/**
 * @file config/config.h
 * @brief 配置统一初始化入口
 */
#pragma once

namespace config
{
    /**
     * @brief 统一初始化全部配置。
     *
     * 依次调用 init_env()（环境变量）与 init_cache()（cache.yml）；
     * 任一配置缺失或非法都会打印错误并 exit(1)。
     */
    void init();

} // namespace config
