#!/bin/bash
# ============================================================================
# MODplayer 自动构建脚本
# 自动编译 FFmpeg 集成的媒体服务器
# 使用: ./build.sh [选项]
# ============================================================================

set -e  # 遇到错误立即退出

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
SERVER_DIR="$PROJECT_ROOT/server"
BUILD_DIR="$SERVER_DIR/build"
BIN_DIR="$BUILD_DIR/bin"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # 无颜色

# 日志函数
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_debug() { echo -e "${MAGENTA}[DEBUG]${NC} $1"; }
log_step() { echo -e "${CYAN}[STEP]${NC} $1"; }

# 显示标题
print_header() {
    echo -e "${CYAN}"
    echo "================================================================="
    echo "   MODplayer 媒体服务器 - 自动构建脚本"
    echo "   版本: 1.0.0 | 包含 FFmpeg 集成"
    echo "================================================================="
    echo -e "${NC}"
}

# 显示帮助
show_help() {
    print_header
    echo "用法: $0 [选项]"
    echo
    echo "选项:"
    echo "  -h, --help            显示此帮助信息"
    echo "  -c, --clean           清理构建目录"
    echo "  -d, --debug           使用 Debug 模式编译"
    echo "  -r, --release         使用 Release 模式编译"
    echo "  -t, --test            编译后运行测试"
    echo "  -j, --jobs NUM        指定并行编译作业数"
    echo "  -v, --verbose         显示详细输出"
    echo "  -f, --force           强制重新配置"
    echo "  --install-deps        安装缺失的依赖"
    echo
    echo "示例:"
    echo "  $0                    标准构建"
    echo "  $0 -c -d             清理并使用 Debug 模式构建"
    echo "  $0 -j4 -t            使用4个作业并行编译并运行测试"
    echo "  $0 --install-deps    安装系统依赖"
}

# 解析命令行参数
parse_args() {
    BUILD_TYPE="Release"
    CLEAN_BUILD=false
    RUN_TESTS=false
    VERBOSE=false
    FORCE_RECONFIGURE=false
    INSTALL_DEPS=false
    JOB_COUNT=$(nproc 2>/dev/null || echo 4)
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_help
                exit 0
                ;;
            -c|--clean)
                CLEAN_BUILD=true
                shift
                ;;
            -d|--debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            -r|--release)
                BUILD_TYPE="Release"
                shift
                ;;
            -t|--test)
                RUN_TESTS=true
                shift
                ;;
            -j|--jobs)
                JOB_COUNT="$2"
                shift 2
                ;;
            -v|--verbose)
                VERBOSE=true
                shift
                ;;
            -f|--force)
                FORCE_RECONFIGURE=true
                shift
                ;;
            --install-deps)
                INSTALL_DEPS=true
                shift
                ;;
            *)
                log_error "未知选项: $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # 如果请求安装依赖，优先处理
    if [ "$INSTALL_DEPS" = true ]; then
        install_dependencies
        exit 0
    fi
}

# 检查并安装依赖
install_dependencies() {
    log_step "检查并安装系统依赖..."
    
    if ! command -v sudo >/dev/null 2>&1; then
        log_warn "未找到 sudo 命令，将尝试使用普通权限"
        SUDO=""
    else
        SUDO="sudo"
    fi
    
    # 检测发行版
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
    else
        log_error "无法检测操作系统"
        exit 1
    fi
    
    log_info "检测到操作系统: $OS"
    
    # 安装依赖
    case $OS in
        ubuntu|debian)
            $SUDO apt-get update
            $SUDO apt-get install -y \
                build-essential \
                cmake \
                pkg-config \
                libavcodec-dev \
                libavformat-dev \
                libavutil-dev \
                libavfilter-dev \
                libswscale-dev \
                libavdevice-dev \
                ffmpeg \
                curl
            ;;
        fedora|centos|rhel)
            if command -v dnf >/dev/null 2>&1; then
                $SUDO dnf groupinstall -y "Development Tools"
                $SUDO dnf install -y \
                    cmake \
                    pkgconfig \
                    ffmpeg-devel \
                    curl
            elif command -v yum >/dev/null 2>&1; then
                $SUDO yum groupinstall -y "Development Tools"
                $SUDO yum install -y \
                    cmake \
                    pkgconfig \
                    ffmpeg-devel \
                    curl
            fi
            ;;
        arch|manjaro)
            $SUDO pacman -Syu --noconfirm \
                base-devel \
                cmake \
                pkg-config \
                ffmpeg \
                curl
            ;;
        *)
            log_warn "不支持的操作系统: $OS"
            log_info "请手动安装以下依赖:"
            echo "  - 构建工具: gcc, g++, make, cmake, pkg-config"
            echo "  - FFmpeg 开发库: libavcodec, libavformat, libavutil, libavfilter, libswscale"
            echo "  - 工具: ffmpeg, curl"
            read -p "按 Enter 键继续，或按 Ctrl+C 取消..."
            ;;
    esac
    
    log_success "依赖安装完成"
}

# 检查依赖
check_dependencies() {
    log_step "检查构建依赖..."
    
    local missing=()
    
    # 检查构建工具
    for cmd in cmake g++ make pkg-config; do
        if ! command -v $cmd >/dev/null 2>&1; then
            missing+=("$cmd")
        fi
    done
    
    # 检查FFmpeg开发库
    if ! pkg-config --exists libavcodec libavformat libavutil; then
        missing+=("FFmpeg开发库 (libavcodec-dev, libavformat-dev, libavutil-dev)")
    fi
    
    # 检查FFmpeg工具
    if ! command -v ffmpeg >/dev/null 2>&1; then
        missing+=("ffmpeg")
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        log_error "缺少以下依赖:"
        for dep in "${missing[@]}"; do
            log_error "  - $dep"
        done
        
        echo
        read -p "是否尝试自动安装依赖? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            install_dependencies
        else
            log_info "可以手动安装依赖，或使用: $0 --install-deps"
            exit 1
        fi
    fi
    
    # 显示版本信息
    log_info "CMake 版本: $(cmake --version | head -n1)"
    log_info "g++ 版本: $(g++ --version | head -n1)"
    
    # 检查FFmpeg版本
    if pkg-config --exists libavcodec; then
        AVCODEC_VERSION=$(pkg-config --modversion libavcodec 2>/dev/null || echo "未知")
        log_info "libavcodec 版本: $AVCODEC_VERSION"
    fi
    
    if command -v ffmpeg >/dev/null 2>&1; then
        FFMPEG_VERSION=$(ffmpeg -version 2>/dev/null | head -n1 | sed 's/^ffmpeg version //' | awk '{print $1}')
        log_info "FFmpeg 版本: $FFMPEG_VERSION"
    fi
    
    log_success "所有依赖检查通过"
}

# 清理构建目录
clean_build() {
    log_step "清理构建目录..."
    
    if [ -d "$BUILD_DIR" ]; then
        if [ "$VERBOSE" = true ]; then
            rm -rfv "$BUILD_DIR"
        else
            rm -rf "$BUILD_DIR"
        fi
        log_success "已清理构建目录: $BUILD_DIR"
    else
        log_info "构建目录不存在，无需清理"
    fi
}

# 创建构建目录
create_build_dir() {
    log_step "准备构建目录..."
    
    mkdir -p "$BUILD_DIR"
    mkdir -p "$BIN_DIR"
    
    log_info "项目根目录: $PROJECT_ROOT"
    log_info "构建目录: $BUILD_DIR"
    log_info "输出目录: $BIN_DIR"
    log_info "构建类型: $BUILD_TYPE"
    
    # 检查源代码文件
    local cpp_files=$(find "$SERVER_DIR/src" -name "*.cpp" 2>/dev/null | wc -l)
    local h_files=$(find "$SERVER_DIR/include" -name "*.h" 2>/dev/null | wc -l)
    
    if [ "$cpp_files" -eq 0 ]; then
        log_error "在 $SERVER_DIR/src 中未找到任何 .cpp 文件"
        exit 1
    fi
    
    log_info "找到 $cpp_files 个 .cpp 源文件"
    log_info "找到 $h_files 个 .h 头文件"
    
    if [ "$VERBOSE" = true ]; then
        log_debug "源文件列表:"
        find "$SERVER_DIR/src" -name "*.cpp" | while read file; do
            log_debug "  - $(basename "$file")"
        done
    fi
}

# 运行 CMake 配置
run_cmake() {
    log_step "运行 CMake 配置..."
    
    cd "$BUILD_DIR"
    
    local CMAKE_ARGS=(
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE"
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$BIN_DIR"
    )
    
    if [ "$VERBOSE" = true ]; then
        CMAKE_ARGS+=("-DCMAKE_VERBOSE_MAKEFILE=ON")
    fi
    
    if [ "$FORCE_RECONFIGURE" = true ] || [ ! -f "$BUILD_DIR/Makefile" ]; then
        log_info "运行 cmake ${CMAKE_ARGS[*]} .."
        if [ "$VERBOSE" = true ]; then
            cmake "${CMAKE_ARGS[@]}" ..
        else
            if ! cmake "${CMAKE_ARGS[@]}" .. > "$BUILD_DIR/cmake.log" 2>&1; then
                log_error "CMake 配置失败"
                log_info "详细日志: $BUILD_DIR/cmake.log"
                cat "$BUILD_DIR/cmake.log"
                exit 1
            fi
        fi
        log_success "CMake 配置成功"
    else
        log_info "使用现有的 CMake 配置"
    fi
}

# 编译项目
compile_project() {
    log_step "编译项目..."
    
    cd "$BUILD_DIR"
    
    local MAKE_ARGS=("-j$JOB_COUNT")
    
    if [ "$VERBOSE" = true ]; then
        MAKE_ARGS+=("VERBOSE=1")
        log_info "运行 make ${MAKE_ARGS[*]}"
        if ! make "${MAKE_ARGS[@]}"; then
            log_error "编译失败"
            exit 1
        fi
    else
        log_info "使用 $JOB_COUNT 个并行作业编译"
        if ! make "${MAKE_ARGS[@]}" > "$BUILD_DIR/make.log" 2>&1; then
            log_error "编译失败"
            log_info "详细日志: $BUILD_DIR/make.log"
            
            # 显示最后50行错误信息
            tail -50 "$BUILD_DIR/make.log"
            
            # 检查常见错误
            if grep -q "fatal error:.*libavcodec/avcodec.h" "$BUILD_DIR/make.log"; then
                log_error "找不到 FFmpeg 头文件。请确保已安装 FFmpeg 开发库。"
                log_info "在 Ubuntu/Debian 上运行: sudo apt install libavcodec-dev libavformat-dev libavutil-dev"
            fi
            
            if grep -q "undefined reference" "$BUILD_DIR/make.log"; then
                log_error "链接错误。请确保 FFmpeg 库正确安装。"
            fi
            
            exit 1
        fi
    fi
    
    # 检查生成的可执行文件
    if [ -f "$BIN_DIR/media_server" ]; then
        local FILE_SIZE=$(du -h "$BIN_DIR/media_server" | cut -f1)
        log_success "编译成功！可执行文件: $BIN_DIR/media_server ($FILE_SIZE)"
    else
        log_error "可执行文件未找到: $BIN_DIR/media_server"
        exit 1
    fi
}

# 运行测试
run_tests() {
    log_step "运行测试..."
    
    if [ ! -f "$BIN_DIR/media_server" ]; then
        log_error "可执行文件不存在，无法运行测试"
        return 1
    fi
    
    # 运行测试程序（如果有）
    if [ -f "$SERVER_DIR/src/test_ffmpeg.cpp" ]; then
        log_info "编译并运行 FFmpeg 测试..."
        
        # 临时编译测试程序
        local TEST_SRC="$SERVER_DIR/src/test_ffmpeg.cpp"
        local TEST_BIN="$BIN_DIR/test_ffmpeg"
        
        # 获取FFmpeg编译标志
        local CFLAGS=$(pkg-config --cflags libavcodec libavformat libavutil 2>/dev/null || echo "")
        local LDFLAGS=$(pkg-config --libs libavcodec libavformat libavutil 2>/dev/null || echo "")
        
        if g++ -std=c++17 $CFLAGS -o "$TEST_BIN" "$TEST_SRC" $LDFLAGS 2>/dev/null; then
            if "$TEST_BIN"; then
                log_success "FFmpeg 测试通过"
            else
                log_warn "FFmpeg 测试失败"
            fi
            rm -f "$TEST_BIN"
        else
            log_warn "无法编译 FFmpeg 测试程序"
        fi
    fi
    
    # 运行简单的功能测试
    log_info "运行功能测试..."
    
    # 检查可执行文件是否能够启动
    timeout 2s "$BIN_DIR/media_server" --version 2>/dev/null && {
        log_success "服务器可执行文件测试通过"
    } || {
        log_warn "服务器可执行文件启动测试失败（这可能是正常的，如果程序需要参数）"
    }
    
    # 检查是否有内存泄漏
    if command -v valgrind >/dev/null 2>&1; then
        log_info "运行内存泄漏检查 (valgrind)..."
        timeout 5s valgrind --leak-check=summary "$BIN_DIR/media_server" --help 2>&1 | grep -q "ERROR SUMMARY: 0 errors" && {
            log_success "内存泄漏检查通过"
        } || {
            log_warn "发现内存泄漏问题"
        }
    fi
}

# 显示构建摘要
show_summary() {
    log_step "构建完成！"
    echo
    echo -e "${GREEN}=========================================================${NC}"
    echo -e "${GREEN}  🎉 MODplayer 媒体服务器构建成功！${NC}"
    echo -e "${GREEN}=========================================================${NC}"
    echo
    echo -e "${CYAN}项目信息:${NC}"
    echo -e "  项目目录:  $PROJECT_ROOT"
    echo -e "  媒体目录:  $PROJECT_ROOT/media"
    echo -e "  Web界面:   $PROJECT_ROOT/web"
    echo
    echo -e "${CYAN}可执行文件:${NC}"
    echo -e "  ${GREEN}$BIN_DIR/media_server${NC}"
    echo
    echo -e "${CYAN}运行方法:${NC}"
    echo -e "  1. 直接运行:"
    echo -e "     ${YELLOW}cd $SERVER_DIR && ./build/bin/media_server${NC}"
    echo
    echo -e "  2. 带参数运行:"
    echo -e "     ${YELLOW}cd $SERVER_DIR && ./build/bin/media_server --port 8080 --media-dir ../media${NC}"
    echo
    echo -e "${CYAN}测试服务器:${NC}"
    echo -e "  1. 启动服务器后，在浏览器中打开:"
    echo -e "     ${YELLOW}http://localhost:8080${NC}"
    echo
    echo -e "  2. API 端点:"
    echo -e "     ${YELLOW}http://localhost:8080/api/status${NC}"
    echo -e "     ${YELLOW}http://localhost:8080/api/media/list${NC}"
    echo
    echo -e "${CYAN}查看日志:${NC}"
    echo -e "  ${YELLOW}tail -f $SERVER_DIR/build/server.log${NC}"
    echo
    echo -e "${GREEN}=========================================================${NC}"
    
    # 创建启动脚本
    cat > "$PROJECT_ROOT/start-server.sh" << EOF
#!/bin/bash
# MODplayer 启动脚本
cd "\$(dirname "\$0")/server"
exec ./build/bin/media_server
EOF
    
    chmod +x "$PROJECT_ROOT/start-server.sh"
    
    echo -e "已创建启动脚本: ${GREEN}$PROJECT_ROOT/start-server.sh${NC}"
    echo
}

# 主函数
main() {
    print_header
    
    parse_args "$@"
    
    # 检查依赖
    check_dependencies
    
    # 清理构建目录（如果需要）
    if [ "$CLEAN_BUILD" = true ]; then
        clean_build
    fi
    
    # 创建构建目录
    create_build_dir
    
    # 运行 CMake
    run_cmake
    
    # 编译项目
    compile_project
    
    # 运行测试（如果需要）
    if [ "$RUN_TESTS" = true ]; then
        run_tests
    fi
    
    # 显示摘要
    show_summary
}

# 运行主函数
main "$@"
