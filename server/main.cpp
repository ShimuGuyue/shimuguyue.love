/**
 * @file main.cpp
 */

#include <iostream>

#include "about/about_init.h"
#include "db/connection.h"
#include "http/routes.h"
#include "image/image_queries.h"
#include "md/markdown_parser.h"

int main(int argc, char* argv[])
{
    std::ios::sync_with_stdio(false);

    about::check_readme_dir();

    const std::string host = http::read_host_or_exit();
    const int         port = http::read_port_or_exit();

    pqxx::connection conn = db::connect();

    md::init();
    img::init();

    httplib::Server svr;
    http::setup_routes(svr, conn);

    svr.listen(host, port);

    return 0;
}
