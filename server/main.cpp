/**
 * @file main.cpp
 */

#include <iostream>
#include <spdlog/spdlog.h>

#include "db/connection.h"
#include "doc/blog_queries.h"
#include "http/routes.h"
#include "img/image_queries.h"

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::info("项目初始化进行中...");

    http::init();   // HTTP 路由前置配置
    db::  init();   // 数据库连接与表检查
    doc:: init();   // 文档相关初始化
    img:: init();   // 图片相关初始化

    httplib::Server svr;
    http::setup_routes(svr, db::connection());

    spdlog::info("项目初始化完成。\n");

    svr.listen(http::server_host(), http::server_port());
}
