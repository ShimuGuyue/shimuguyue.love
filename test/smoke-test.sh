#!/usr/bin/env bash
# ============================================================
# test/smoke-test.sh — 后端冒烟测试（CI 与本地共用）
# ============================================================
# 用法：./test/smoke-test.sh
#       默认使用 server/build/server；CI 或自定义路径时用 SERVER_BIN 指定，
#       如 SERVER_BIN=server-build/server bash test/smoke-test.sh
# 依赖工具：psql、redis-cli、curl
#
# 约定：使用 conf/.env（本地已有，或由 CI 在调用前生成）；
#       表已存在跳过建表；端口已有旧服务端则先终止，保证使用临时进程；
#       验证公开 GET 接口返回正常、Redis 缓存写入以及空结果不缓存。
# ============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# ---- 配置 ----
SERVER_BIN="${SERVER_BIN:-$REPO_ROOT/server/build/server}"
START_TIMEOUT="${START_TIMEOUT:-30}"
LOG_FILE="${SMOKE_LOG_FILE:-$REPO_ROOT/test/smoke-server.log}"

cd "$REPO_ROOT"

# ---- 加载 conf/.env ----
# 服务端只读 conf/.env 文件（不读进程环境变量），项目必须存在该文件
ENV_FILE="$REPO_ROOT/conf/.env"
if [[ ! -f "$ENV_FILE" ]]; then
    echo "[smoke] 未找到 ${ENV_FILE}，请先创建（CI 会在调用脚本前自动生成）" >&2
    exit 1
fi
set -a
# shellcheck disable=SC1091
source "$ENV_FILE"
set +a
echo "[smoke] 使用 conf/.env。"

BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"
# psql 连接参数与 Redis 参数均取自 conf/.env
DB_ARGS=(-h "$PGHOST" -p "$PGPORT" -U "$PGUSER" -d "$PGDATABASE")
export PGPASSWORD

echo "[smoke] 测试配置：API=${BASE_URL}  服务端=${SERVER_BIN}"
echo "[smoke] 测试配置：PG=${PGHOST}:${PGPORT}/${PGDATABASE}  Redis=${REDIS_HOST}:${REDIS_PORT}"
echo "[smoke] 测试配置：START_TIMEOUT=${START_TIMEOUT}  日志=${LOG_FILE}"

# ---- 依赖工具检查 ----
for tool in psql redis-cli curl; do
    if ! command -v "$tool" > /dev/null 2>&1; then
        echo "[smoke] 缺少依赖工具：${tool}" >&2
        exit 1
    fi
done

# ---- 数据库表初始化（幂等） ----
# 以 users 表是否存在作为“已初始化”标志，避免重复执行 CREATE TABLE 报错
if psql "${DB_ARGS[@]}" -tAc \
    "SELECT 1 FROM pg_tables WHERE schemaname='public' AND tablename='users'" \
    | grep -q 1; then
    echo "[smoke] 数据库 ${PGHOST}:${PGPORT}/${PGDATABASE} 表已存在，跳过初始化。"
else
    echo "[smoke] 初始化数据库表（${PGHOST}:${PGPORT}/${PGDATABASE}）..."
    for f in sql/create_*.sql; do
        echo "[smoke] 初始化 ${f}"
        psql "${DB_ARGS[@]}" -v ON_ERROR_STOP=1 -f "$REPO_ROOT/$f"
    done
fi

# ---- 种子数据（幂等） ----
# 空结果不写缓存的策略下，空库的列表接口不会生成缓存键；
# 写入最小测试数据，保证公开列表接口都有非空结果、缓存键验证仍然有效。
echo "[smoke] 写入列表接口种子数据..."
psql "${DB_ARGS[@]}" -v ON_ERROR_STOP=1 <<'SQL' > /dev/null
INSERT INTO categories (name) VALUES ('测试分类')
ON CONFLICT (name) DO NOTHING;
INSERT INTO tags (name, category_id)
SELECT '测试标签', id FROM categories WHERE name = '测试分类'
ON CONFLICT (name, category_id) DO NOTHING;
INSERT INTO blogs (title, description, content, file_path, category_id)
SELECT '测试博客', '', 'smoke content', 'smoke-test-blog', id
FROM categories WHERE name = '测试分类'
ON CONFLICT (file_path) DO NOTHING;
INSERT INTO images (path, description) VALUES ('home/smoke.jpg', 'smoke')
ON CONFLICT (path) DO NOTHING;
SQL

# ---- 启动临时服务端（终止端口上已有进程，保证测试用本脚本创建的进程） ----
# 探测用 curl 必须带超时：未监听端口在某些环境会静默丢包而非快速拒绝
SERVER_PID=""
if curl -sf --connect-timeout 2 --max-time 5 "$BASE_URL/api/about" > /dev/null 2>&1; then
    echo "[smoke] ${BASE_URL} 已有服务端在运行，先终止旧进程。"
    OLD_PID=$(ss -ltnp 2>/dev/null \
        | sed -n "s/.*:${SERVER_PORT} .*pid=\([0-9][0-9]*\).*/\1/p" \
        | head -1)
    if [[ -z "$OLD_PID" ]]; then
        # 兜底：按服务端二进制路径匹配进程
        OLD_PID=$(pgrep -f "^${SERVER_BIN}" 2>/dev/null | head -1 || true)
    fi
    if [[ -z "$OLD_PID" ]]; then
        echo "[smoke] ${BASE_URL} 被占用但无法定位占用进程（PID），请手动停止后重试" >&2
        exit 1
    fi
    echo "[smoke] 终止旧服务端（PID=${OLD_PID}）。"
    kill "$OLD_PID" 2>/dev/null || true
    for _ in $(seq 1 10); do
        if ! curl -sf --connect-timeout 1 --max-time 2 "$BASE_URL/api/about" > /dev/null 2>&1; then
            break
        fi
        sleep 1
    done
    if kill -0 "$OLD_PID" 2>/dev/null; then
        echo "[smoke] 旧服务端未退出，强制终止。"
        kill -9 "$OLD_PID" 2>/dev/null || true
        sleep 1
    fi
    if curl -sf --connect-timeout 1 --max-time 2 "$BASE_URL/api/about" > /dev/null 2>&1; then
        echo "[smoke] 端口 ${SERVER_PORT} 仍被占用，无法启动临时服务端" >&2
        exit 1
    fi
fi

chmod +x "$SERVER_BIN"
"$SERVER_BIN" > "$LOG_FILE" 2>&1 &
SERVER_PID=$!
echo "[smoke] 临时服务端已启动（PID=${SERVER_PID}），等待就绪..."

READY=false
for _ in $(seq 1 "$START_TIMEOUT"); do
    if curl -sf --connect-timeout 2 --max-time 5 "$BASE_URL/api/about" > /dev/null 2>&1; then
        READY=true
        break
    fi
    sleep 1
done
if [[ "$READY" != true ]]; then
    echo "[smoke] 服务端启动超时，日志如下：" >&2
    cat "$LOG_FILE" >&2
    kill "$SERVER_PID" 2>/dev/null || true
    exit 1
fi
echo "[smoke] 服务端已就绪。"

# 统一清理：临时服务端、临时缓存键列表
cleanup() {
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        echo "[smoke] 临时服务端已停止（PID=${SERVER_PID}）。"
    fi
    rm -f "${CACHE_KEYS_FILE:-}"
}
trap cleanup EXIT

# ---- 公开 GET 接口冒烟测试 ----
for ep in images blogs categories tags about; do
    code=$(curl -s -o /dev/null -w '%{http_code}' \
        --connect-timeout 2 --max-time 10 "$BASE_URL/api/${ep}")
    if [[ "$code" != "200" ]]; then
        echo "[smoke] GET /api/${ep} 失败（HTTP ${code}）" >&2
        exit 1
    fi
    echo "[smoke] GET /api/${ep} -> 200 OK"
done

# ---- 验证公开 GET 接口缓存写入 ----
# SCAN 拉取全部 api-cache:* 键后逐键精确比对，确保五个接口都已写缓存
REDIS_ARGS=(-h "$REDIS_HOST" -p "$REDIS_PORT")
if [[ -n "${REDIS_PASSWORD:-}" ]]; then
    REDIS_ARGS+=(-a "$REDIS_PASSWORD")
fi

# 键列表先落盘再比对，失败时可整体打印
CACHE_KEYS_FILE=$(mktemp)

redis-cli "${REDIS_ARGS[@]}" --scan --pattern 'api-cache:*' > "$CACHE_KEYS_FILE"
FOUND=$(wc -l < "$CACHE_KEYS_FILE")
echo "[smoke] Redis（${REDIS_HOST}:${REDIS_PORT}）扫描到 ${FOUND} 个缓存键。"
for key in \
    api-cache:/api/images \
    api-cache:/api/blogs \
    api-cache:/api/categories \
    api-cache:/api/tags \
    api-cache:/api/about; do
    if grep -qxF "$key" "$CACHE_KEYS_FILE"; then
        echo "[smoke] 缓存键存在：${key}"
    else
        echo "[smoke] 缓存键缺失：${key}" >&2
        cat "$CACHE_KEYS_FILE" >&2
        exit 1
    fi
done
echo "[smoke] 五个公开接口的缓存键均已写入。"

# ---- 空结果不缓存验证 ----
curl -sf "$BASE_URL/api/blogs?q=__smoke_no_match__" > /dev/null
if redis-cli "${REDIS_ARGS[@]}" exists 'api-cache:/api/blogs?q=__smoke_no_match__' \
    | grep -qx 0; then
    echo "[smoke] 空结果未写入缓存（符合预期）。"
else
    echo "[smoke] 空结果被写入了缓存" >&2
    exit 1
fi

echo "[smoke] 后端冒烟测试全部通过（5 个公开接口、5 个缓存键）。"
