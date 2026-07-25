/**
 * @file about/about_init.cpp
 * @brief About 页面初始化实现
 */
#include "about/about_init.h"

#include <cstdlib>
#include <iostream>

namespace about {

void check_readme_dir()
{
    /*log*/std::cout << "正在获取 README_DIR..." << std::endl;
    if (!std::getenv("README_DIR"))
    {
        /*log*/std::cerr << "错误：缺少必需的环境变量 README_DIR！" << std::endl;
        std::exit(1);
    }
    /*log*/std::cout << "README_DIR 获取成功。\n" << std::endl;
}

} // namespace about
