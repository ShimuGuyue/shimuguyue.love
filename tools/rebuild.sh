#!/bin/bash

# tools/rebuild.sh — 一键重构：前端构建 → 后端构建 → 重启服务
# 用法: ./tools/rebuild.sh

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$SCRIPT_DIR/.."
SERVER_RUN="$SCRIPT_DIR/server-run.sh"

echo "===== [1/4] 前端构建 (npm) ====="
(cd "$ROOT/client" && npm run build)

echo "===== [2/4] 后端构建 (cmake) ====="
(cd "$ROOT/server" && cmake -B build --preset default)
(cd "$ROOT/server" && cmake --build build)

echo "===== [3/4] 停止旧服务 ====="
"$SERVER_RUN" stop 

echo "===== [4/4] 启动新服务 ====="
"$SERVER_RUN" start

echo "===== 重构完成 ====="
