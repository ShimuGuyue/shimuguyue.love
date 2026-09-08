#!/bin/bash

# tools/rebuild.sh — 一键重构：git pull → 后端构建 → 重启服务 → 前端构建
# 用法: ./tools/rebuild.sh

set -euo pipefail
set -x

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$SCRIPT_DIR/.."
SERVER_RUN="$SCRIPT_DIR/server-run.sh"

echo "===== [1/5] 拉取最新代码 ====="
(cd "$ROOT" && git pull)

echo "===== [2/5] 后端构建 (cmake) ====="
(cd "$ROOT/server" && cmake -B build --preset release)
(cd "$ROOT/server" && cmake --build build)

echo "===== [3/5] 停止旧服务 ====="
"$SERVER_RUN" stop

echo "===== [4/5] 启动新服务 ====="
"$SERVER_RUN" start

echo "===== [5/5] 前端构建 (npm) ====="
(cd "$ROOT/client" && npm install)
(cd "$ROOT/client" && npm run build)

echo "===== 重构完成 ====="
