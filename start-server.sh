#!/bin/bash
# MODplayer 启动脚本
cd "$(dirname "$0")/server"
exec ./build/bin/media_server
