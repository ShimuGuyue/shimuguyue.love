/**
 * @file config/cache.h
 * @brief 各项公开接口缓存有效期（conf/cache.yml）配置
 */
#pragma once

namespace config
{
    /**
     * @brief 加载并校验 conf/cache.yml。
     *
     * 在 conf/ 目录（由 find_conf_dir() 统一查找）内读取 cache.yml；
     * 文件缺失、字段缺失或数值非法时打印错误并 exit(1)。
     * 校验通过的有效期写入 ConfigMap 统一存储，键名见下（单位：秒）：
     *   categories → CACHE_TTL_CATEGORIES  分类列表
     *   tags       → CACHE_TTL_TAGS        标签列表
     *   blogs      → CACHE_TTL_BLOGS       博客列表
     *   blog       → CACHE_TTL_BLOG        博客详情
     *   images     → CACHE_TTL_IMAGES      照片墙
     */
    void init_cache();

} // namespace config
