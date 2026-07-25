#!/bin/bash

# ./serverctl.sh stop  停止
# ./serverctl.sh start 启动

# 服务相关配置
APP="./build/server"          # 可执行文件路径
PID_FILE="./server.pid"       # 存放进程 ID 的文件
LOG_FILE="./server.log"       # 输出日志文件

# 启动服务
start() {
    if [ -f "$PID_FILE" ] && kill -0 $(cat "$PID_FILE") 2>/dev/null; then
        echo "服务已在运行 (PID: $(cat $PID_FILE))"
        return 1
    fi

    echo "正在启动服务..."
    # 后台运行，重定向标准输出和错误到日志文件
    nohup $APP >> "$LOG_FILE" 2>&1 &
    # 记录 PID
    echo $! > "$PID_FILE"
    echo "服务启动成功，PID: $(cat $PID_FILE)"
}

# 停止服务
stop() {
    if [ ! -f "$PID_FILE" ]; then
        echo "PID 文件不存在，服务可能未运行"
        return 1
    fi

    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        echo "正在停止服务 (PID: $PID)..."
        kill "$PID"
        # 等待进程结束
        sleep 2
        if kill -0 "$PID" 2>/dev/null; then
            echo "进程未响应，强制终止..."
            kill -9 "$PID"
        fi
        rm -f "$PID_FILE"
        echo "服务已停止"
    else
        echo "进程不存在，清理 PID 文件"
        rm -f "$PID_FILE"
    fi
}

# 查看服务状态
status() {
    if [ -f "$PID_FILE" ] && kill -0 $(cat "$PID_FILE") 2>/dev/null; then
        echo "服务正在运行，PID: $(cat $PID_FILE)"
    else
        echo "服务未运行"
        if [ -f "$PID_FILE" ]; then
            echo "但存在过时的 PID 文件，建议执行 stop 清理"
        fi
    fi
}

# 重启服务
restart() {
    stop
    start
}

# 命令行参数处理
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    status)
        status
        ;;
    restart)
        restart
        ;;
    *)
        echo "用法: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac

exit 0