/**
 * @file config/env.cpp
 * @brief 环境变量加载、初始化与存储实现
 */

#include "config/env.h"
#include "config/env_map.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include <spdlog/spdlog.h>

namespace
{
/// 必需的环境变量列表。
constexpr std::string_view REQUIRED_KEYS[] = {
    "FRONTEND_ORIGIN",
    "SERVER_HOST",
    "SERVER_PORT",
    "FILE_PATH",
    "FIXED_SALT",
    "PGHOST",
    "PGPORT",
    "PGDATABASE",
    "PGUSER",
    "PGPASSWORD",
    "DB_POOL_SIZE",
    "SESSION_TTL_MINUTES",
    "REDIS_HOST",
    "REDIS_PORT",
    "REDIS_POOL_SIZE",
};

    /**
     * @brief 去除字符串首尾的空白字符。
     * @param s 原始字符串。
     * @return 去除空白后的字符串。
     */
    auto trim(std::string_view s) -> std::string_view
    {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
            s.remove_prefix(1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
            s.remove_suffix(1);
        return s;
    }

    /**
     * @brief 从当前目录向上查找项目的 conf/.env 文件。
     * @return conf/.env 文件路径；未找到返回 std::nullopt。
     */
    auto find_env_file() -> std::optional<std::filesystem::path>
    {
        std::filesystem::path dir{ std::filesystem::current_path() };
        for (;;)
        {
            const auto candidate = dir / "conf" / ".env";
            if (std::filesystem::is_regular_file(candidate))
                return candidate;

            const auto parent = dir.parent_path();
            if (parent == dir)
                return std::nullopt;
            dir = parent;
        }
    }

    /**
     * @brief 解析 .env 文件并写入环境变量存储。
     * @param path .env 文件路径。
     * @param env  环境变量存储。
     */
    void load_env_file(const std::filesystem::path& path, config::EnvMap& env)
    {
        std::ifstream ifs{ path };
        if (!ifs)
        {
            spdlog::error("无法读取 .env 文件：{}", path.string());
            std::exit(1);
        }

        std::string line;
        while (std::getline(ifs, line))
        {
            auto view = trim(line);
            if (view.empty() || view.front() == '#')
                continue;

            if (view.starts_with("export "))
                view.remove_prefix(7);

            const auto eq = view.find('=');
            if (eq == std::string_view::npos)
            {
                spdlog::warn("忽略无效的 .env 行：{}", line);
                continue;
            }

            auto key   = trim(view.substr(0, eq));
            auto value = trim(view.substr(eq + 1));
            if (value.size() >= 2
            &&  ((value.front() == '"' && value.back() == '"')
              || (value.front() == '\'' && value.back() == '\'')))
            {
                value.remove_prefix(1);
                value.remove_suffix(1);
            }
            env.set(std::string{ key }, std::string{ value });
        }
    }

} // namespace





namespace config
{
    void init_env()
    {
        // 查找并加载项目的 .env 文件
        const auto env_file = find_env_file();
        if (!env_file)
        {
            spdlog::error("未找到 conf/.env 文件！请将 .env 放在项目 conf/ 目录中。");
            std::exit(1);
        }
        load_env_file(*env_file, EnvMap::env_values);

        // 校验必需的环境变量
        for (const auto& key : REQUIRED_KEYS)
        {
            if (EnvMap::env_values[std::string{ key }].empty())
            {
                spdlog::error("缺少必需的环境变量 {}！", key);
                std::exit(1);
            }
        }

        // SERVER_PORT 必须是 1~65535 的端口号
        {
            const auto& server_port = EnvMap::env_values["SERVER_PORT"];
            unsigned int parsed     = 0;
            const auto [ptr, ec]    = std::from_chars(
                server_port.data(),
                server_port.data() + server_port.size(),
                parsed
            );
            if (ec != std::errc{} || ptr != server_port.data() + server_port.size()
            ||  parsed == 0 || parsed > 65535)
            {
                spdlog::error("环境变量 SERVER_PORT 必须是有效的端口号！");
                std::exit(1);
            }
        }

        // 校验 FIXED_SALT：16 字节盐值的 hex 编码（32 个 hex 字符）
        const auto& fixed_salt = EnvMap::env_values["FIXED_SALT"];
        const bool fixed_salt_valid{
            fixed_salt.size() == 32 &&
            std::all_of(fixed_salt.begin(), fixed_salt.end(),
                [](unsigned char c)
                {
                    return (c >= '0' && c <= '9')
                        || (c >= 'a' && c <= 'f')
                        || (c >= 'A' && c <= 'F');
                }
            )
        };
        if (!fixed_salt_valid)
        {
            spdlog::error("环境变量 FIXED_SALT 必须是 32 位十六进制字符串（16 字节盐值）！");
            std::exit(1);
        }

        // DB_POOL_SIZE 必须是正整数
        {
            const auto& pool_size = EnvMap::env_values["DB_POOL_SIZE"];
            std::size_t parsed    = 0;
            const auto [ptr, ec]  = std::from_chars(
                pool_size.data(),
                pool_size.data() + pool_size.size(),
                parsed
            );
            if (ec != std::errc{} || ptr != pool_size.data() + pool_size.size() || parsed == 0)
            {
                spdlog::error("环境变量 DB_POOL_SIZE 必须是正整数！");
                std::exit(1);
            }
        }

        // SESSION_TTL_MINUTES 必须是正整数
        {
            const auto& ttl_minutes = EnvMap::env_values["SESSION_TTL_MINUTES"];
            std::size_t parsed      = 0;
            const auto [ptr, ec]    = std::from_chars(
                ttl_minutes.data(),
                ttl_minutes.data() + ttl_minutes.size(),
                parsed
            );
            if (ec != std::errc{} || ptr != ttl_minutes.data() + ttl_minutes.size() || parsed == 0)
            {
                spdlog::error("环境变量 SESSION_TTL_MINUTES 必须是正整数！");
                std::exit(1);
            }
        }

        // REDIS_PORT 必须是 1~65535 的端口号
        {
            const auto& redis_port = EnvMap::env_values["REDIS_PORT"];
            unsigned int parsed    = 0;
            const auto [ptr, ec]   = std::from_chars(
                redis_port.data(),
                redis_port.data() + redis_port.size(),
                parsed
            );
            if (ec != std::errc{} || ptr != redis_port.data() + redis_port.size()
            ||  parsed == 0 || parsed > 65535)
            {
                spdlog::error("环境变量 REDIS_PORT 必须是有效的端口号！");
                std::exit(1);
            }
        }

        // REDIS_POOL_SIZE 必须是正整数
        {
            const auto& pool_size = EnvMap::env_values["REDIS_POOL_SIZE"];
            std::size_t parsed   = 0;
            const auto [ptr, ec] = std::from_chars(
                pool_size.data(),
                pool_size.data() + pool_size.size(),
                parsed
            );
            if (ec != std::errc{} || ptr != pool_size.data() + pool_size.size() || parsed == 0)
            {
                spdlog::error("环境变量 REDIS_POOL_SIZE 必须是正整数！");
                std::exit(1);
            }
        }

        // 统一创建并检测 FILE_PATH 下的所有文件目录
        const auto root = std::filesystem::path{ EnvMap::env_values["FILE_PATH"] };
        const std::filesystem::path SUBDIRS[] = {
            "doc",
            "doc/blogs",
            "image",
            "image/home",
            "doc/README",
        };

        std::error_code ec;
        if (!std::filesystem::create_directories(root, ec) && ec)
        {
            spdlog::error("文件根目录不可用：{}（{}）", root.string(), ec.message());
            std::exit(1);
        }
        for (const auto& sub : SUBDIRS)
        {
            const auto dir = root / sub;
            std::error_code sub_ec;
            if (!std::filesystem::create_directories(dir, sub_ec) && sub_ec)
            {
                spdlog::error("目录不可用：{}（{}）", dir.string(), sub_ec.message());
                std::exit(1);
            }
        }

        spdlog::info("文件目录已确认：${FILE_PATH}/doc/blogs、${FILE_PATH}/image/home、${FILE_PATH}/doc/README。");

        spdlog::info("环境变量已加载。");
    }

    auto env() -> const EnvMap&
    {
        return EnvMap::instance();
    }

}
