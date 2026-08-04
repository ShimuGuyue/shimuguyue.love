/**
 * @file main.cpp
 */

#include <iostream>
#include <spdlog/spdlog.h>

#include "config/env.h"
#include "db/connection.h"
#include "http/routes.h"

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::info("项目初始化进行中...");

    config::init(); // 环境变量初始化
    db::    init(); // 数据库连接与表检查

    httplib::Server svr;
    http::setup_routes(svr, db::connection());

    spdlog::info("项目初始化完成。\n");

    svr.listen(config::env()["SERVER_HOST"], std::stoi(config::env()["SERVER_PORT"]));
}
