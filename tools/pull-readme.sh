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
#   2. 本脚本对 $FILE_PATH/doc/README 目录有写权限。
#   3. git 已安装。
#   4. psql 已安装并配置好数据库连接环境变量（PGHOST 等）。
#   5. curl 已安装；如需缓存失效回退，另需 redis-cli（redis-tools）。
#
# 环境变量：
#   GITHUB_USER    — GitHub 用户名，仓库地址为 github.com/$GITHUB_USER/$GITHUB_USER
#   FILE_PATH      — 文件根目录，README 仓库本地存放路径为 $FILE_PATH/doc/README
#   BRANCH         — 拉取的分支名（默认 main）
#
# 工作原理：
#   首次运行时 clone 仓库到 $FILE_PATH/doc/README。
#   之后每次运行执行 git fetch + reset --hard 获取最新内容。
#   拉取成功后通过 psql 将内容写入数据库 about 表。
#   前端 GET /api/about 从数据库直接读取，不再依赖文件系统。
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

# 同步内容到数据库
README_FILE="$FILE_PATH/doc/README/README.md"
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
    # 重建 /api/about 缓存：先失效旧键，再请求接口触发服务端回源写缓存。
    # 若仅请求接口，旧缓存命中时会直接返回旧内容，不会真正重建。
    ABOUT_URL="http://127.0.0.1:${SERVER_PORT:-8080}/api/about"
    CACHE_KEY="api-cache:/api/about"

    invalidated=false
    if command -v redis-cli > /dev/null 2>&1; then
        REDIS_ARGS=()
        if [[ -n "${REDIS_PASSWORD:-}" ]]; then
            REDIS_ARGS+=(-a "$REDIS_PASSWORD")
        fi
        if redis-cli "${REDIS_ARGS[@]}" DEL "$CACHE_KEY" > /dev/null 2>&1; then
            invalidated=true
        fi
    fi

    if curl -sf "$ABOUT_URL" > /dev/null 2>&1; then
        if [[ "$invalidated" == true ]]; then
            echo "[pull-readme] 已重建 /api/about 缓存。"
        else
            echo "[pull-readme] 警告：旧缓存未失效，/api/about 可能仍返回旧缓存内容。" >&2
        fi
    else
        if [[ "$invalidated" == true ]]; then
            echo "[pull-readme] 警告：服务端不可达，已失效 /api/about 缓存，将在下次请求时重建。" >&2
        else
            echo "[pull-readme] 警告：缓存重建失败（服务端不可达且旧缓存未失效），内容将在 TTL 后自动过期。" >&2
        fi
    fi
else
    echo "[pull-readme] 警告：同步到数据库失败！" >&2
fi
rm -f "$TMP_SQL"

echo "[pull-readme] $(date '+%Y-%m-%d %H:%M') 完成。"
