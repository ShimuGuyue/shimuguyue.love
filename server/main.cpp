/**
 * @file main.cpp
 */

#include <iostream>
#include <spdlog/spdlog.h>

#include "db/connection.h"
#include "http/routes.h"
#include "img/image_queries.h"
#include "md/markdown_parser.h"

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::info("项目初始化进行中...");

    http::init();
    db::  init();
    md::  init();
    img:: init();

    httplib::Server svr;
    http::setup_routes(svr, db::connection());

    spdlog::info("项目初始化完成。\n");

    svr.listen(http::server_host(), http::server_port());
}
