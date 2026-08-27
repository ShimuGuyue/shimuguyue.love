/**
 * @file main.cpp
 */

#include <csignal>
#include <iostream>
#include <spdlog/spdlog.h>

#include "config/env.h"
#include "db/connection.h"
#include "http/routes.h"

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::info("项目初始化进行中...");

    // 忽略 SIGPIPE：数据库断连写入失败时防止进程被信号终止（libpqxx 文档建议）
    std::signal(SIGPIPE, SIG_IGN);

    config::init(); // 环境变量初始化
    db::    init(); // 数据库连接池与表检查

    httplib::Server svr;

    svr.set_keep_alive_timeout(std::chrono::seconds(30));
    svr.set_read_timeout(std::chrono::seconds(30));
    svr.set_write_timeout(std::chrono::seconds(30));

    http::setup_routes(svr);

    spdlog::info("项目初始化完成。\n");

    svr.listen(config::env()["SERVER_HOST"], std::stoi(config::env()["SERVER_PORT"]));
}
