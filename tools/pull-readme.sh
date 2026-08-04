#!/usr/bin/env bash
# ============================================================
# pull-readme.sh — 个人介绍 README 自动拉取脚本
# ============================================================
# 用途：从 GitHub 拉取 $GITHUB_USER/$GITHUB_USER 仓库到本地，
#       并通过 psql 将 README.md 内容同步到数据库 about 表。
# 部署：由 crontab 或 systemd timer 每天定时调用一次。
#
# 前置条件：
#   1. 目标仓库公开可访问（无需认证）。
#   2. 本脚本对 $README_DIR 目录有写权限。
#   3. git 已安装。
#   4. psql 已安装并配置好数据库连接环境变量（PGHOST 等）。
#
# 环境变量：
#   GITHUB_USER    — GitHub 用户名，仓库地址为 github.com/$GITHUB_USER/$GITHUB_USER
#   README_DIR     — 仓库本地存放路径
#   BRANCH         — 拉取的分支名（默认 main）
#
# 工作原理：
#   首次运行时 clone 仓库到 $README_DIR。
#   之后每次运行执行 git fetch + reset --hard 获取最新内容。
#   拉取成功后通过 psql 将内容写入数据库 about 表。
#   前端 GET /api/about 从数据库直接读取，不再依赖文件系统。
# ============================================================

set -euo pipefail

# ---- 加载项目 .env（项目根目录） ----
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -f "$PROJECT_ROOT/.env" ]]; then
    set -a
    # shellcheck disable=SC1091
    source "$PROJECT_ROOT/.env"
    set +a
fi

# ---- 配置 ----
GITHUB_USER="${GITHUB_USER:?GITHUB_USER 未设置，请检查项目根目录的 .env 文件}"
README_DIR="${README_DIR:?README_DIR 未设置，请检查项目根目录的 .env 文件}"
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

# 同步内容到数据库
README_FILE="$README_DIR/README.md"
if [[ ! -f "$README_FILE" ]]; then
    echo "[pull-readme] 错误：$README_FILE 不存在！" >&2
    exit 1
fi

echo "[pull-readme] 正在同步 README 内容到数据库..."
TMP_SQL=$(mktemp)
{
    printf "UPDATE about SET content = \$content\$"
    cat "$README_FILE"
    printf "\$content\$ WHERE id = 1;\n"
} > "$TMP_SQL"

if psql -f "$TMP_SQL" > /dev/null 2>&1; then
    echo "[pull-readme] 已同步 README 内容到数据库。"
else
    echo "[pull-readme] 警告：同步到数据库失败！" >&2
fi
rm -f "$TMP_SQL"

echo "[pull-readme] $(date '+%Y-%m-%d %H:%M') 完成。"
