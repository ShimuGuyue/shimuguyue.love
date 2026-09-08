#!/usr/bin/env bash
# ============================================================
# pull-readme.sh — 个人介绍 README 自动拉取脚本
# ============================================================
# 用途：从 GitHub 拉取 $GITHUB_USER/$GITHUB_USER 仓库到本地。
#       前端构建时由 client/vite.config.ts 直接读取
#       $FILE_PATH/doc/README/README.md，生成《关于我》静态内容。
# 部署：由 tools/rebuild.sh 在每次 npm build 前调用；也可手动执行。
#
# 前置条件：
#   1. 目标仓库公开可访问（无需认证）。
#   2. 本脚本对 $FILE_PATH/doc/README 目录有写权限。
#   3. git 已安装。
#
# 环境变量：
#   GITHUB_USER    — GitHub 用户名，仓库地址为 github.com/$GITHUB_USER/$GITHUB_USER
#   FILE_PATH      — 文件根目录，README 仓库本地存放路径为 $FILE_PATH/doc/README
#   BRANCH         — 拉取的分支名（默认 main）
#
# 工作原理：
#   首次运行时 clone 仓库到 $FILE_PATH/doc/README。
#   之后每次运行执行 git fetch + reset --hard 获取最新内容。
#   每次 npm build 前调用，保证构建进页面的 README 为最新内容。
# ============================================================

set -euo pipefail

# ---- 加载项目 conf/.env ----
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -f "$PROJECT_ROOT/conf/.env" ]]; then
    set -a
    # shellcheck disable=SC1091
    source "$PROJECT_ROOT/conf/.env"
    set +a
fi

# ---- 配置 ----
GITHUB_USER="${GITHUB_USER:?GITHUB_USER 未设置，请检查项目 conf/ 目录的 .env 文件}"
FILE_PATH="${FILE_PATH:?FILE_PATH 未设置，请检查项目 conf/ 目录的 .env 文件}"
REPO_URL="https://github.com/${GITHUB_USER}/${GITHUB_USER}.git"
BRANCH="${BRANCH:-main}"

# 设置时区为东八区
export TZ="Asia/Shanghai"

echo "[pull-readme] $(date '+%Y-%m-%d %H:%M') 开始拉取 README 仓库..."

if [[ -d "$FILE_PATH/doc/README/.git" ]]; then
    git -C "$FILE_PATH/doc/README" fetch origin "$BRANCH"
    git -C "$FILE_PATH/doc/README" reset --hard "origin/$BRANCH"
    echo "[pull-readme] 已更新 README 仓库。"
else
    git clone --depth 1 --branch "$BRANCH" "$REPO_URL" "$FILE_PATH/doc/README"
    echo "[pull-readme] 已 clone README 仓库。"
fi

# 检查拉取结果
README_FILE="$FILE_PATH/doc/README/README.md"
if [[ ! -f "$README_FILE" ]]; then
    echo "[pull-readme] 错误：$README_FILE 不存在！" >&2
    exit 1
fi

echo "[pull-readme] $(date '+%Y-%m-%d %H:%M') 完成。"
