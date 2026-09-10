/**
 * @file config/page_size.h
 * @brief 各分页列表每页条数（conf/page_size.yml）配置
 */
#pragma once

namespace config
{
    /**
     * @brief 加载并校验 conf/page_size.yml。
     *
     * 在 conf/ 目录（由 find_conf_dir() 统一查找）内读取 page_size.yml；
     * 文件缺失、字段缺失或数值非法时打印错误并 exit(1)。
     * 校验通过的每页条数写入 ConfigMap 统一存储，键名见下：
     *   blogs → BLOGS_PAGESIZE
     */
    void init_page_size();

} // namespace config
