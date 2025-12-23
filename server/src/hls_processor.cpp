// server/src/hls_processor_real.cpp
#include "hls_processor.h"
#include "ffmpeg_transcoder.h"
#include <iostream>
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

class HLSProcessor::Impl {
public:
    struct StreamData {
        std::unique_ptr<FFmpegTranscoder> transcoder;
        std::string media_id;
        std::string media_path;
        HLSStreamConfig config;
        int viewers = 0;
    };
    
    std::map<std::string, StreamData> streams;
    mutable std::mutex mutex;
};

std::map<std::string, std::string> HLSStreamStatus::to_json() const {
    std::map<std::string, std::string> json;
    json["stream_id"] = stream_id;
    json["media_id"] = media_id;
    json["status"] = status;
    json["error_message"] = error_message;
    json["segments_generated"] = std::to_string(segments_generated);
    json["total_segments"] = std::to_string(total_segments);
    json["progress"] = std::to_string(progress);
    json["viewers"] = std::to_string(viewers);
    return json;
}

HLSProcessor::HLSProcessor() : impl_(std::make_unique<Impl>()) {
    std::cout << "[HLS] HLSProcessor 初始化 (真实转码版本)" << std::endl;
    
    // 创建HLS目录
    fs::create_directories("../media/hls");
    fs::create_directories("../media/hls/streams");
}

HLSProcessor::~HLSProcessor() {
    std::cout << "[HLS] HLSProcessor 清理" << std::endl;
    
    // 停止所有转码器
    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto& pair : impl_->streams) {
        if (pair.second.transcoder) {
            pair.second.transcoder->stop();
        }
    }
    impl_->streams.clear();
}

HLSProcessor& HLSProcessor::get_instance() {
    static HLSProcessor instance;
    return instance;
}

bool HLSProcessor::create_stream(const std::string& media_path, 
                                const std::string& media_id,
                                const HLSStreamConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    
    // 生成流ID
    std::string stream_id = config.stream_id;
    if (stream_id.empty()) {
        static int counter = 0;
        stream_id = "stream_" + std::to_string(++counter);
    }
    
    // 检查是否已存在
    if (impl_->streams.find(stream_id) != impl_->streams.end()) {
        std::cout << "[HLS] 流已存在: " << stream_id << std::endl;
        return true;
    }
    
    // 检查媒体文件
    if (!fs::exists(media_path)) {
        std::cerr << "[HLS] 媒体文件不存在: " << media_path << std::endl;
        return false;
    }
    
    // 创建转码配置
    HLSStreamConfig stream_config = config;
    stream_config.stream_id = stream_id;
    stream_config.media_path = media_path;
    stream_config.media_id = media_id;
    
    // 设置输出目录
    std::string output_dir = "../media/hls/streams/" + stream_id;
    stream_config.output_dir = output_dir;
    stream_config.playlist_path = output_dir + "/playlist.m3u8";
    stream_config.segment_prefix = "segment";
    
    // 创建转码器配置
    TranscodeConfig transcode_config;
    transcode_config.input_path = media_path;
    transcode_config.output_dir = output_dir;
    transcode_config.stream_id = stream_id;
    transcode_config.video_bitrate = stream_config.video_bitrate;
    transcode_config.audio_bitrate = stream_config.audio_bitrate;
    transcode_config.segment_duration = stream_config.segment_duration;
    transcode_config.max_segments = stream_config.max_segments;
    transcode_config.resolution = stream_config.resolution;
    
    // 创建并启动转码器
    auto transcoder = std::make_unique<FFmpegTranscoder>(transcode_config);
    if (!transcoder->start()) {
        std::cerr << "[HLS] 无法启动转码器: " << stream_id << std::endl;
        return false;
    }
    
    // 保存流数据
    impl_->streams[stream_id] = {
        .transcoder = std::move(transcoder),
        .media_id = media_id,
        .media_path = media_path,
        .config = stream_config
    };
    
    std::cout << "[HLS] 实时转码流创建成功: " << stream_id << std::endl;
    std::cout << "[HLS] 输出目录: " << output_dir << std::endl;
    std::cout << "[HLS] 播放列表: " << stream_config.playlist_path << std::endl;
    
    return true;
}

HLSStreamStatus HLSProcessor::get_stream_status(const std::string& stream_id) const {
    HLSStreamStatus status;
    status.stream_id = stream_id;
    
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->streams.find(stream_id);
    if (it != impl_->streams.end()) {
        const auto& stream_data = it->second;
        status.media_id = stream_data.media_id;
        status.viewers = stream_data.viewers;
        
        if (stream_data.transcoder) {
            std::string transcoder_status = stream_data.transcoder->get_status();
            if (transcoder_status.find("transcoding") != std::string::npos) {
                status.status = "transcoding";
            } else if (transcoder_status.find("error") != std::string::npos) {
                status.status = "error";
                status.error_message = transcoder_status;
            } else {
                status.status = "ready";
            }
            
            status.segments_generated = stream_data.transcoder->get_segment_count();
            status.total_segments = 100; // 可以基于时长估算
            
            if (status.segments_generated > 0) {
                status.progress = (double)status.segments_generated / status.total_segments;
            }
        } else {
            status.status = "error";
            status.error_message = "Transcoder not available";
        }
    } else {
        status.status = "not_found";
        status.error_message = "Stream not found";
    }
    
    return status;
}

std::string HLSProcessor::get_playlist(const std::string& stream_id) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->streams.find(stream_id);
    if (it != impl_->streams.end() && it->second.transcoder) {
        return it->second.transcoder->get_playlist();
    }
    return "";
}

std::vector<char> HLSProcessor::get_segment(const std::string& stream_id, 
                                          const std::string& segment_name) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->streams.find(stream_id);
    if (it != impl_->streams.end() && it->second.transcoder) {
        // 增加观看者计数
        it->second.viewers++;
        
        auto segment_data = it->second.transcoder->get_segment(segment_name);
        
        // 减少观看者计数
        it->second.viewers--;
        
        return segment_data;
    }
    return {};
}

std::vector<std::string> HLSProcessor::list_streams() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    std::vector<std::string> streams;
    for (const auto& pair : impl_->streams) {
        streams.push_back(pair.first);
    }
    return streams;
}

bool HLSProcessor::stop_stream(const std::string& stream_id) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto it = impl_->streams.find(stream_id);
    if (it != impl_->streams.end()) {
        if (it->second.transcoder) {
            it->second.transcoder->stop();
        }
        
        // 清理目录
        std::string output_dir = "../media/hls/streams/" + stream_id;
        try {
            if (fs::exists(output_dir)) {
                fs::remove_all(output_dir);
            }
        } catch (...) {
            // 忽略清理错误
        }
        
        impl_->streams.erase(it);
        std::cout << "[HLS] 流已停止: " << stream_id << std::endl;
        return true;
    }
    return false;
}