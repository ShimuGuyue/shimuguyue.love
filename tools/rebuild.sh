#!/bin/bash

# tools/rebuild.sh — 一键重构：前端构建 → 后端构建 → 重启服务
# 用法: ./tools/rebuild.sh

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$SCRIPT_DIR/.."
SERVER_RUN="$SCRIPT_DIR/server-run.sh"

echo "===== [1/5] 拉取最新代码 ====="
echo "git pull"
(cd "$ROOT" && git pull)

echo "===== [2/5] 前端构建 (npm) ====="
echo "npm install"
echo "npm run build"
(cd "$ROOT/client" && npm install && npm run build)

echo "===== [3/5] 后端构建 (cmake) ====="
echo "cmake -B build --preset default"
(cd "$ROOT/server" && cmake -B build --preset default)
echo "cmake --build build"
(cd "$ROOT/server" && cmake --build build)

echo "===== [4/5] 停止旧服务 ====="
echo "$SERVER_RUN" stop
"$SERVER_RUN" stop

echo "===== [5/5] 启动新服务 ====="
echo "$SERVER_RUN" start
"$SERVER_RUN" start

echo "===== 重构完成 ====="
