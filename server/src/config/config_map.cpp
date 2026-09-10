/**
 * @file config/config_map.cpp
 * @brief 配置项存储封装实现
 */

#include "config/config_map.h"

#include <utility>

namespace config
{
    /// 全局唯一实例的定义。
    ConfigMap ConfigMap::config_values;

    auto ConfigMap::instance() -> const ConfigMap&
    {
        return config_values;
    }

    auto ConfigMap::operator[](const std::string& key) const -> const std::string&
    {
        static const std::string EMPTY;
        const auto it = values_.find(key);
        return it == values_.end() ? EMPTY : it->second;
    }

    void ConfigMap::set(const std::string& key, std::string value)
    {
        values_.insert_or_assign(key, std::move(value));
    }

} // namespace config
