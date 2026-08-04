/**
 * @file config/env_map.h
 * @brief 环境变量存储封装
 */
#pragma once

#include <string>
#include <unordered_map>

namespace config
{
/**
 * @brief 环境变量存储封装类。
 *
 * 内部使用 std::unordered_map 存储环境变量，
 * 仅提供读取（operator[]）和写入（set）接口。
 * 全局唯一实例以静态成员 env_values 存储，通过 instance() 只读访问。
 */
class EnvMap
{
private:
    friend void init();

    EnvMap() = default;
    EnvMap(const EnvMap&) = delete;
    EnvMap& operator=(const EnvMap&) = delete;

    static EnvMap env_values;
    std::unordered_map<std::string, std::string> values_;

public:
    /**
     * @brief 获取全局唯一实例（只读）。
     * @return 环境变量存储的只读引用。
     */
    [[nodiscard]] static auto instance() -> const EnvMap&;

    /**
     * @brief 读取环境变量的值。
     * @param key 环境变量名。
     * @return 环境变量的值；不存在时返回空字符串。
     */
    [[nodiscard]] auto operator[](const std::string& key) const -> const std::string&;

    /**
     * @brief 写入环境变量的值。
     * @param key   环境变量名。
     * @param value 环境变量的值。
     */
    void set(const std::string& key, std::string value);

};

} // namespace config
