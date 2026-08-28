/**
 * @file config/cache.h
 * @brief 各项公开接口缓存有效期（conf/cache.yml）配置
 */
#pragma once
#include <cstdint>

namespace config
{
    /**
     * @brief 各项公开接口缓存的过期时间（秒）。
     */
    struct CacheTtl
    {
        int64_t categories; ///< 分类列表
        int64_t tags;       ///< 标签列表
        int64_t blogs;      ///< 博客列表
        int64_t blog;       ///< 博客详情
        int64_t images;     ///< 照片墙
        int64_t about;      ///< 关于我
        int64_t profile;    ///< 个人介绍
    };

    /**
     * @brief 加载并校验 conf/cache.yml。
     *
     * 从当前目录向上查找 conf/cache.yml；
     * 文件缺失、字段缺失或数值非法时打印错误并 exit(1)。
     */
    void init_cache();

    /**
     * @brief 获取缓存有效期配置。
     * @return 只读引用，须在 init_cache() 之后调用。
     */
    [[nodiscard]] auto cache_ttl() -> const CacheTtl&;

} // namespace config
