#!/usr/bin/env bash
# ============================================================
# pull-friend-links.sh — 友情链接数据仓库自动拉取脚本
# ============================================================
# 用途：从环境变量 FRIENDS_REPO 指定的 Git 仓库拉取友链数据到本地。
#       仓库内 meta.yaml 记录友链条目（id / title / url / description），
#       同目录下 ${id}.* 为对应头像。前端构建时由 client/vite.config.ts
#       直接读取 $FILE_PATH/friend_links/meta.yaml，生成《友情链接》静态内容。
# 部署：由 tools/rebuild.sh 在每次 npm build 前调用；也可手动执行。
#
# 前置条件：
#   1. 目标仓库公开可访问（无需认证）。
#   2. 本脚本对 $FILE_PATH/friend_links 目录有写权限。
#   3. git 已安装。
#
# 环境变量：
#   FILE_PATH — 文件根目录，友链仓库本地存放路径为 $FILE_PATH/friend_links
#   FRIENDS_REPO — 友链数据仓库地址（Git HTTPS 仓库 URL，必填）
#
# 工作原理：
#   - 目录内已有 .git：git fetch + reset --hard 获取最新内容。
#   - 目录不存在：git clone 仓库。
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
FILE_PATH="${FILE_PATH:?FILE_PATH 未设置，请检查项目 conf/ 目录的 .env 文件}"
FRIENDS_REPO="${FRIENDS_REPO:?FRIENDS_REPO 未设置，请检查项目 conf/ 目录的 .env 文件}"
FRIEND_DIR="$FILE_PATH/friend_links"

# 设置时区为东八区
export TZ="Asia/Shanghai"

echo "[pull-friend-links] $(date '+%Y-%m-%d %H:%M') 开始拉取友链仓库..."

if [[ -d "$FRIEND_DIR/.git" ]]; then
    git -C "$FRIEND_DIR" remote set-url origin "$FRIENDS_REPO"
    git -C "$FRIEND_DIR" fetch --depth 1 origin main
    git -C "$FRIEND_DIR" reset --hard "origin/main"
    echo "[pull-friend-links] 已更新友链仓库。"
else
    git clone --depth 1 --branch main "$FRIENDS_REPO" "$FRIEND_DIR"
    echo "[pull-friend-links] 已 clone 友链仓库。"
fi

# 检查拉取结果
META_FILE="$FRIEND_DIR/meta.yaml"
if [[ ! -f "$META_FILE" ]]; then
    echo "[pull-friend-links] 错误：$META_FILE 不存在！" >&2
    exit 1
fi

echo "[pull-friend-links] $(date '+%Y-%m-%d %H:%M') 完成。"
