#!/usr/bin/env bash
# ============================================================
# auto-sync-blogs.sh — 博客内容自动同步脚本
# ============================================================
# 用途：检测 $FILE_PATH/doc/blogs/ 下 .md 文件变更，自动 git add / commit / push
# 部署：由 crontab 或 systemd timer 定时每分钟调用一次
#
# 前置条件：
#   1. $FILE_PATH/doc/blogs 是一个已初始化的 git 仓库，含远程仓库配置
#   2. 本脚本需要有对 $FILE_PATH/doc/blogs 的写权限
#   3. git 已配置 user.name 和 user.email（用于 auto-commit）
#
# 环境变量：
#   FILE_PATH         — 文件根目录，博客内容根目录为 $FILE_PATH/doc
#   SYNC_REMOTE       — 推送到的远程仓库名（默认 origin）
#   SYNC_BRANCH       — 推送到的分支名（默认 auto；本地不在该分支时自动切换）
#
# 工作原理：
#   $FILE_PATH/doc/blogs 是独立的 git 仓库。
#   每次运行时，暂存已跟踪文件的修改/删除，以及新增或修改的 .md 文件；
#   未跟踪的非 .md 文件（临时文件等）不参与同步。
#   如果有变更，执行 git add / commit / push。
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
FILE_PATH="${FILE_PATH:-}"
SYNC_REMOTE="${SYNC_REMOTE:-origin}"
SYNC_BRANCH="${SYNC_BRANCH:-auto}"

# 设置时区为东八区
export TZ="Asia/Shanghai"

DRY_RUN=false
FORCE=false
for arg in "${@}"; do
    case "$arg" in
        -n|--dry-run) DRY_RUN=true ;;
        -f|--force)   FORCE=true ;;
    esac
done

# ---- 校验 ----
if [[ -z "$FILE_PATH" ]]; then
    echo "[auto-sync] 错误：环境变量 FILE_PATH 未设置。"
    exit 1
fi

GIT_REPO="$FILE_PATH/doc/blogs"

if [[ ! -d "$GIT_REPO" ]]; then
    echo "[auto-sync] 提示：$GIT_REPO 目录不存在，跳过。"
    exit 0
fi

cd "$GIT_REPO"

# 确保是一个 git 仓库
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "[auto-sync] 错误：$GIT_REPO 不是一个 git 仓库。"
    exit 1
fi

# 目标分支固定为 auto（本地分支不一致时后续自动切换）
BRANCH="$SYNC_BRANCH"

if $DRY_RUN; then
    echo "[auto-sync] [DRY-RUN] 检测到以下变更："
    git status --short || true
    echo "[auto-sync] [DRY-RUN] 将执行: git checkout $BRANCH && git add -u && git add '*.md' && git commit && git push $SYNC_REMOTE $BRANCH"
    exit 0
fi

# ---- 确保使用目标分支 ----
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "")
if [[ -n "$CURRENT_BRANCH" && "$CURRENT_BRANCH" != "$BRANCH" ]]; then
    echo "[auto-sync] 当前分支 $CURRENT_BRANCH 与目标 $BRANCH 不一致，切换到 $BRANCH。"
    if git show-ref --verify --quiet "refs/heads/$BRANCH"; then
        git checkout "$BRANCH"
    elif git show-ref --verify --quiet "refs/remotes/$SYNC_REMOTE/$BRANCH"; then
        git checkout -b "$BRANCH" "$SYNC_REMOTE/$BRANCH"
    else
        git checkout -b "$BRANCH"
    fi
fi

# ---- 变更检测与暂存 ----
# 仅暂存：已跟踪文件的修改/删除 + 新增或修改的 .md 文件；
# 未跟踪的非 .md 文件（临时文件等）不参与同步
git add -u
git add '*.md'

if ! $FORCE && git diff --cached --quiet; then
    echo "[auto-sync] $(date '+%Y-%m-%d %H:%M') 无变更。"
    exit 0
fi

# ---- Git 操作 ----
echo "[auto-sync] $(date '+%Y-%m-%d %H:%M') 检测到博客文件变更，准备同步..."

# Git commit
COMMIT_MSG="$(date '+%y-%m-%d:%H:%M')"
if git commit -m "$COMMIT_MSG" --no-verify; then
    echo "[auto-sync] 提交成功: $COMMIT_MSG"
else
    echo "[auto-sync] 提交失败（可能无变更）。"
    exit 1
fi

# Git push
if git push "$SYNC_REMOTE" "$BRANCH"; then
    echo "[auto-sync] 推送成功 → $SYNC_REMOTE/$BRANCH"
else
    echo "[auto-sync] 推送失败！请检查网络或仓库配置。"
    exit 1
fi

echo "[auto-sync] $(date '+%Y-%m-%d %H:%M') 同步完成。"
