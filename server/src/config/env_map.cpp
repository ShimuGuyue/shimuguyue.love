/**
 * @file config/env_map.cpp
 * @brief 环境变量存储封装实现
 */

#include "config/env_map.h"

#include <utility>

namespace config
{
    /// 全局唯一实例的定义。
    EnvMap EnvMap::env_values;

    auto EnvMap::instance() -> const EnvMap&
    {
        return env_values;
    }

    auto EnvMap::operator[](const std::string& key) const -> const std::string&
    {
        static const std::string EMPTY;
        const auto it = values_.find(key);
        return it == values_.end() ? EMPTY : it->second;
    }

    void EnvMap::set(const std::string& key, std::string value)
    {
        values_.insert_or_assign(key, std::move(value));
    }

} // namespace config
