#!/bin/bash

# tools/rebuild.sh — 一键重构：git pull → 后端构建 → 重启服务 → 拉取 README/友链 → 前端构建
# 用法: ./tools/rebuild.sh

set -euo pipefail
set -x

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$SCRIPT_DIR/.."
SERVER_RUN="$SCRIPT_DIR/server-run.sh"

echo "===== [1/7] 拉取最新代码 ====="
(cd "$ROOT" && git pull)

echo "===== [2/7] 后端构建 (cmake) ====="
(cd "$ROOT/server" && cmake -B build --preset release)
(cd "$ROOT/server" && cmake --build build)

echo "===== [3/7] 停止旧服务 ====="
"$SERVER_RUN" stop

echo "===== [4/7] 启动新服务 ====="
"$SERVER_RUN" start

echo "===== [5/7] 拉取 README（《关于我》内容） ====="
bash "$ROOT/tools/pull-readme.sh"

echo "===== [6/7] 拉取友链仓库（《友情链接》内容） ====="
bash "$ROOT/tools/pull-friend-links.sh"

echo "===== [7/7] 前端构建 (npm) ====="
(cd "$ROOT/client" && npm install)
(cd "$ROOT/client" && npm run build)

echo "===== 重构完成 ====="
