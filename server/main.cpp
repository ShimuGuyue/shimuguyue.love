/**
 * @file main.cpp
 */

#include <iostream>
#include <spdlog/spdlog.h>

#include "db/connection.h"
#include "http/routes.h"
#include "image/image_queries.h"
#include "md/markdown_parser.h"

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::trace);
    spdlog::info("项目初始化进行中...\n");

    http::init();

    pqxx::connection conn = db::connect();

    md::init();
    img::init();

    httplib::Server svr;
    http::setup_routes(svr, conn);

    svr.listen(http::server_host(), http::server_port());
}
