// server/src/ffmpeg_transcoder.cpp
#include "ffmpeg_transcoder.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <filesystem>

namespace fs = std::filesystem;

FFmpegTranscoder::FFmpegTranscoder(const TranscodeConfig& config) 
    : config_(config) {
    std::cout << "[FFmpeg] 创建转码器: " << config.stream_id << std::endl;
}

FFmpegTranscoder::~FFmpegTranscoder() {
    stop();
    cleanup();
}

bool FFmpegTranscoder::create_output_directory() {
    try {
        // 创建输出目录
        fs::create_directories(config_.output_dir);
        fs::create_directories(config_.output_dir + "/segments");
        return true;
    } catch (const std::exception& e) {
        error_message_ = std::string("创建目录失败: ") + e.what();
        return false;
    }
}

std::string FFmpegTranscoder::build_ffmpeg_command() const {
    std::stringstream cmd;
    
    cmd << "ffmpeg -y -i \"" << config_.input_path << "\" "
        << "-c:v libx264 -c:a aac "
        << "-preset ultrafast "
        << "-crf 23 "
        << "-maxrate 2000k -bufsize 4000k "
        << "-pix_fmt yuv420p "
        << "-g 48 -keyint_min 48 "
        << "-sc_threshold 0 "
        << "-hls_time " << config_.segment_duration << " "
        << "-hls_list_size 0 "
        << "-hls_flags delete_segments "
        << "-hls_playlist_type vod "
        << "-hls_segment_filename \"" << config_.output_dir << "/segments/segment_%03d.ts\" "
        << "\"" << config_.output_dir << "/playlist.m3u8\"";
    
    if (!config_.enable_logging) {
        cmd << " 2>/dev/null";
    }
    
    return cmd.str();
}

void FFmpegTranscoder::transcode_process() {
    std::cout << "[FFmpeg] 开始转码: " << config_.stream_id << std::endl;
    
    // 构建命令
    std::string cmd = build_ffmpeg_command();
    
    if (config_.enable_logging) {
        std::cout << "[FFmpeg] 命令: " << cmd << std::endl;
    }
    
    // 执行命令
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        error_message_ = "无法启动FFmpeg进程";
        is_running_ = false;
        return;
    }
    
    // 获取进程ID
    ffmpeg_pid_ = 1; // 简化处理
    
    char buffer[256];
    while (is_running_ && fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (config_.enable_logging) {
            std::cout << "[FFmpeg] " << config_.stream_id << ": " << buffer;
        }
        
        // 更新分片计数
        std::string segments_dir = config_.output_dir + "/segments";
        try {
            int count = 0;
            for (const auto& entry : fs::directory_iterator(segments_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".ts") {
                    count++;
                }
            }
            segment_count_ = count;
        } catch (...) {
            // 忽略目录读取错误
        }
    }
    
    pclose(pipe);
    ffmpeg_pid_ = -1;
    
    std::cout << "[FFmpeg] 转码结束: " << config_.stream_id << std::endl;
}

bool FFmpegTranscoder::start() {
    if (is_running_) {
        return true;
    }
    
    // 创建输出目录
    if (!create_output_directory()) {
        return false;
    }
    
    // 检查输入文件
    if (!fs::exists(config_.input_path)) {
        error_message_ = "输入文件不存在: " + config_.input_path;
        return false;
    }
    
    is_running_ = true;
    
    // 启动转码线程
    transcode_thread_ = std::thread(&FFmpegTranscoder::transcode_process, this);
    
    // 等待一段时间，让FFmpeg开始
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    return true;
}

void FFmpegTranscoder::stop() {
    if (!is_running_) {
        return;
    }
    
    is_running_ = false;
    
    // 终止FFmpeg进程
    if (ffmpeg_pid_ > 0) {
        kill(ffmpeg_pid_, SIGTERM);
    }
    
    // 等待线程结束
    if (transcode_thread_.joinable()) {
        transcode_thread_.join();
    }
}

void FFmpegTranscoder::cleanup() {
    // 清理临时文件
    try {
        if (fs::exists(config_.output_dir)) {
            // 可以在这里删除临时文件
        }
    } catch (...) {
        // 忽略清理错误
    }
}

bool FFmpegTranscoder::is_running() const {
    return is_running_;
}

std::string FFmpegTranscoder::get_status() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    if (!error_message_.empty()) {
        return "error: " + error_message_;
    }
    
    if (is_running_) {
        return "transcoding";
    }
    
    return "stopped";
}

int FFmpegTranscoder::get_segment_count() const {
    return segment_count_;
}

std::string FFmpegTranscoder::get_playlist() const {
    std::string playlist_path = config_.output_dir + "/playlist.m3u8";
    
    if (!fs::exists(playlist_path)) {
        return "";
    }
    
    try {
        std::ifstream file(playlist_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    } catch (...) {
        return "";
    }
}

std::vector<char> FFmpegTranscoder::get_segment(const std::string& segment_name) const {
    std::string segment_path = config_.output_dir + "/segments/" + segment_name;
    
    if (!fs::exists(segment_path)) {
        return {};
    }
    
    try {
        std::ifstream file(segment_path, std::ios::binary | std::ios::ate);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<char> buffer(size);
        if (file.read(buffer.data(), size)) {
            return buffer;
        }
    } catch (...) {
        // 读取失败
    }
    
    return {};
}