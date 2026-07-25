#!/usr/bin/env bash
# ============================================================
# pull-readme.sh — 个人介绍 README 自动拉取脚本
# ============================================================
# 用途：从 GitHub 拉取 $GITHUB_USER/$GITHUB_USER 仓库到本地，供 About 页面渲染。
# 部署：由 crontab 或 systemd timer 每天定时调用一次。
#
# 前置条件：
#   1. 目标仓库公开可访问（无需认证）。
#   2. 本脚本对 $README_DIR 目录有写权限。
#   3. git 已安装。
#
# 环境变量：
#   GITHUB_USER    — GitHub 用户名，仓库地址为 github.com/$GITHUB_USER/$GITHUB_USER
#   README_DIR     — 仓库本地存放路径
#   BRANCH         — 拉取的分支名（默认 main）
#
# 工作原理：
#   首次运行时 clone 仓库到 $README_DIR。
#   之后每次运行执行 git fetch + reset --hard 获取最新内容。
#   服务器 GET /api/about 读取 $README_DIR/README.md 返回前端。
# ============================================================

set -euo pipefail

# ---- 配置 ----
GITHUB_USER="${GITHUB_USER:?请设置 GITHUB_USER 环境变量}"
README_DIR="${README_DIR:?请设置 README_DIR 环境变量（仓库存放目录）}"
REPO_URL="https://github.com/${GITHUB_USER}/${GITHUB_USER}.git"
BRANCH="${BRANCH:-main}"

# 设置时区为东八区
export TZ="Asia/Shanghai"

echo "[pull-readme] $(date '+%Y-%m-%d %H:%M') 开始拉取 README 仓库..."

if [[ -d "$README_DIR/.git" ]]; then
    git -C "$README_DIR" fetch origin "$BRANCH"
    git -C "$README_DIR" reset --hard "origin/$BRANCH"
    echo "[pull-readme] 已更新 README 仓库。"
else
    git clone --depth 1 --branch "$BRANCH" "$REPO_URL" "$README_DIR"
    echo "[pull-readme] 已 clone README 仓库。"
fi

echo "[pull-readme] $(date '+%Y-%m-%d %H:%M') 完成。"