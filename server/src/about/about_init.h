/**
 * @file about/about_init.h
 * @brief About 页面初始化
 */
#pragma once

namespace about {

/// 检查 README_DIR 环境变量，未设置则输出错误并退出
void check_readme_dir();

} // namespace about
