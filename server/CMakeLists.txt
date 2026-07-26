cmake_minimum_required(VERSION 3.20)

project(server)

# C++23 版本标准，强制使用版本标准，禁用编译器独有拓展
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg 依赖
find_package(libpqxx           CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(httplib           CONFIG REQUIRED)
find_package(nlohmann_json     CONFIG REQUIRED)
find_package(yaml-cpp          CONFIG REQUIRED)

# 源文件
add_executable(${PROJECT_NAME}
    main.cpp
    src/about/about_queries.cpp
    src/db/connection.cpp
    src/crypto/argon2id.cpp
    src/auth/login.cpp
    src/auth/session.cpp
    src/blog/blog_queries.cpp
    src/image/image_queries.cpp
    src/http/routes.cpp
    src/http/rate_limit.cpp
    src/md/markdown_parser.cpp
    src/profile/profile_queries.cpp
)

# 头文件
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 链接文件
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        libpqxx::pqxx
        unofficial-sodium::sodium
        httplib::httplib
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
)

# ---------- 安装规则 ----------
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
