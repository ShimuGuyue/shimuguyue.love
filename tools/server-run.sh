#!/bin/bash

# ./server-run.sh stop  停止
# ./server-run.sh start 启动

# 服务相关配置
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
APP="$SCRIPT_DIR/../server/build/server"  # 可执行文件路径
LOG_FILE="$SCRIPT_DIR/server-run.log"     # 输出日志文件

# 启动服务
start() {
    # 检查服务是否已经在运行
    RUNNING_PID=$(pgrep -f "$APP" | head -1)
    if [ -n "$RUNNING_PID" ]; then
        echo "服务已在运行 (PID: $RUNNING_PID)，请先停止"
        return 1
    fi

    echo "正在启动服务..."
    # 后台运行，重定向标准输出和错误到日志文件
    nohup $APP >> "$LOG_FILE" 2>&1 &
    # 记录 PID 到日志文件，同时输出到终端
    echo "服务启动成功，PID: $!" | tee -a "$LOG_FILE"
}

# 停止服务
stop() {
    # 通过进程名查找 PID
    PID=$(pgrep -f "$APP" | head -1)
    if [ -z "$PID" ]; then
        echo "服务未运行"
        return 1
    fi

    echo "正在停止服务 (PID: $PID)..."
    kill "$PID"
    # 等待进程结束
    sleep 2
    if kill -0 "$PID" 2>/dev/null; then
        echo "进程未响应，强制终止..."
        kill -9 "$PID"
    fi
    echo "服务已停止"
}

# 命令行参数处理
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    *)
        echo "用法: $0 {start|stop}"
        exit 1
        ;;
esac

exit 0
