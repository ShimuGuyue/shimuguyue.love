/**
 * @file config/config_map.h
 * @brief 配置项存储封装
 */
#pragma once

#include <string>
#include <unordered_map>

namespace config
{
/**
 * @brief 配置项存储封装类。
 *
 * 统一存储全部配置：conf/.env 的环境变量，以及 conf/cache.yml、
 * conf/page_size.yml 中经校验的配置项（键名见各配置模块的注释），
 * 内部使用 std::unordered_map 存储，键值均为字符串。
 * 仅提供读取（operator[]）和写入（set）接口。
 * 全局唯一实例以静态成员 config_values 存储，通过 instance() 只读访问。
 */
class ConfigMap
{
private:
    friend void init_env();
    friend void init_cache();
    friend void init_page_size();

    ConfigMap() = default;
    ConfigMap(const ConfigMap&) = delete;
    ConfigMap& operator=(const ConfigMap&) = delete;

    static ConfigMap config_values;
    std::unordered_map<std::string, std::string> values_;

public:
    /**
     * @brief 获取全局唯一实例（只读）。
     * @return 配置项存储的只读引用。
     */
    [[nodiscard]] static auto instance() -> const ConfigMap&;

    /**
     * @brief 读取配置项的值。
     * @param key 配置项键名。
     * @return 配置项的值；不存在时返回空字符串。
     */
    [[nodiscard]] auto operator[](const std::string& key) const -> const std::string&;

    /**
     * @brief 写入配置项的值。
     * @param key   配置项键名。
     * @param value 配置项的值。
     */
    void set(const std::string& key, std::string value);

};

} // namespace config
