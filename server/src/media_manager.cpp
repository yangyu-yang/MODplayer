#include "media_manager.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cctype>
#include <memory>
#include <atomic>

namespace fs = std::filesystem;

// ============================================================================
// MediaFile 方法实现
// ============================================================================

std::map<std::string, std::string> MediaFile::to_json() const {
    std::map<std::string, std::string> json;
    
    json["id"] = id;
    json["filename"] = filename;
    json["path"] = path;
    json["size"] = std::to_string(size);
    json["duration"] = std::to_string(duration);
    json["format"] = format;
    json["video_codec"] = video_codec;
    json["audio_codec"] = audio_codec;
    json["width"] = std::to_string(width);
    json["height"] = std::to_string(height);
    json["frame_rate"] = std::to_string(frame_rate);
    json["bitrate"] = std::to_string(bitrate);
    json["audio_sample_rate"] = std::to_string(audio_sample_rate);
    json["audio_channels"] = std::to_string(audio_channels);
    json["channel_layout"] = channel_layout;
    json["created_time"] = created_time;
    
    // 添加元数据
    for (const auto& [key, value] : metadata) {
        json["meta_" + key] = value;
    }
    
    return json;
}

MediaFile MediaFile::from_media_info(const MediaInfo& info, const std::string& filename, 
                                    const std::string& path, uint64_t size) {
    MediaFile file;
    
    file.filename = filename;
    file.path = path;
    file.size = size;
    file.duration = info.duration;
    file.format = info.format_name;
    file.bitrate = info.bit_rate;
    file.metadata = info.metadata;
    file.streams = info.streams;
    
    // video info
    file.video_codec = info.video_codec.empty() ? "unknown" : info.video_codec;
    file.width = (info.video_width > 0 && info.video_width <= 100000) ? info.video_width : 0;
    file.height = (info.video_height > 0 && info.video_height <= 100000) ? info.video_height : 0;
    file.frame_rate = (info.video_frame_rate > 0 && info.video_frame_rate <= 1000) ? info.video_frame_rate : 0;

    // audio info
    file.audio_codec = info.audio_codec.empty() ? "unknown" : info.audio_codec;
    file.audio_sample_rate = (info.audio_sample_rate > 0 && info.audio_sample_rate <= 384000) ? info.audio_sample_rate : 0;
    file.audio_channels = (info.audio_channels > 0 && info.audio_channels <= 100) ? info.audio_channels : 0;

    // 根据声道数设置声道布局
    if (info.audio_channels > 0) {
        switch (info.audio_channels) {
            case 1: file.channel_layout = "mono"; break;
            case 2: file.channel_layout = "stereo"; break;
            case 6: file.channel_layout = "5.1"; break;
            case 8: file.channel_layout = "7.1"; break;
            default: file.channel_layout = std::to_string(info.audio_channels) + " channels";
        }
    } else {
        file.channel_layout = "unknown";
    }
    
    return file;
}

// ============================================================================
// MediaManager 方法实现
// ============================================================================

MediaManager::MediaManager() {
    analyzer_ = std::make_unique<MediaAnalyzer>();
}

MediaManager& MediaManager::get_instance() {
    static MediaManager instance;
    return instance;
}

bool MediaManager::scan_directory(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            std::cerr << "Error: Directory does not exist: " << path << std::endl;
            return false;
        }
        
        media_files_.clear();
        media_map_.clear();
        
        std::cout << "========================================" << std::endl;
        std::cout << "Scanning media directory with FFmpeg:" << std::endl;
        std::cout << "  Path: " << path << std::endl;
        std::cout << "========================================" << std::endl;
        
        int processed = 0;
        int successful = 0;
        int skipped = 0;
        
        for (const auto& entry : fs::recursive_directory_iterator(path, 
             fs::directory_options::skip_permission_denied)) {
            
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                std::string filepath = entry.path().string();
                
                if (is_media_file(filename)) {
                    processed++;
                    
                    try {
                        // 使用FFmpeg分析媒体文件
                        MediaInfo media_info = analyzer_->analyze(filepath);
                        
                        if (media_info.success) {
                            // 创建MediaFile对象
                            MediaFile media_file = MediaFile::from_media_info(
                                media_info, filename, filepath, entry.file_size()
                            );
                            
                            // 设置ID
                            media_file.id = generate_id();
                            
                            // 设置创建时间
                            auto ftime = entry.last_write_time();
                            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                            auto cftime = std::chrono::system_clock::to_time_t(sctp);
                            
                            std::stringstream ss;
                            ss << std::put_time(std::localtime(&cftime), "%Y-%m-%d %H:%M:%S");
                            media_file.created_time = ss.str();
                            
                            // 添加到列表
                            media_files_.push_back(media_file);
                            media_map_[media_file.id] = &media_files_.back();
                            successful++;
                            
                            // 输出分析结果
                            std::cout << "✓ [" << successful << "] " << filename << std::endl;
                            std::cout << "  Duration: " << std::fixed << std::setprecision(2) 
                                      << media_file.duration << "s" << std::endl;
                            std::cout << "  Resolution: " << media_file.width << "x" << media_file.height << std::endl;
                            std::cout << "  Video: " << media_file.video_codec << " @ " 
                                      << media_file.frame_rate << "fps" << std::endl;
                            std::cout << "  Audio: " << media_file.audio_codec << " " 
                                      << media_file.audio_sample_rate << "Hz " 
                                      << media_file.channel_layout << std::endl;
                            if (!media_file.metadata.empty()) {
                                std::cout << "  Metadata: " << media_file.metadata.size() << " entries" << std::endl;
                            }
                            std::cout << std::endl;
                            
                        } else {
                            std::cerr << "✗ Failed to analyze: " << filename 
                                      << " - " << media_info.error_message << std::endl;
                            skipped++;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "✗ Exception analyzing: " << filename 
                                  << " - " << e.what() << std::endl;
                        skipped++;
                    }
                }
            }
        }
        
        std::cout << "========================================" << std::endl;
        std::cout << "Scan Summary:" << std::endl;
        std::cout << "  Total processed: " << processed << std::endl;
        std::cout << "  Successfully analyzed: " << successful << std::endl;
        std::cout << "  Failed/Skipped: " << skipped << std::endl;
        std::cout << "  Total in library: " << media_files_.size() << " media files" << std::endl;
        std::cout << "========================================" << std::endl;
        
        return successful > 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
        return false;
    }
}

std::vector<MediaFile> MediaManager::get_all_media() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return media_files_;
}

MediaFile* MediaManager::get_media(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = media_map_.find(id);
    return (it != media_map_.end()) ? it->second : nullptr;
}

MediaFile* MediaManager::get_media_by_name(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& media : media_files_) {
        if (media.filename == filename) {
            return &media;
        }
    }
    return nullptr;
}

std::vector<MediaFile> MediaManager::search(const std::string& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MediaFile> results;
    
    if (query.empty()) {
        return media_files_;
    }
    
    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    for (const auto& media : media_files_) {
        std::string filename_lower = media.filename;
        std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        
        // 搜索文件名
        if (filename_lower.find(query_lower) != std::string::npos) {
            results.push_back(media);
            continue;
        }
        
        // 搜索格式
        std::string format_lower = media.format;
        std::transform(format_lower.begin(), format_lower.end(), format_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (format_lower.find(query_lower) != std::string::npos) {
            results.push_back(media);
            continue;
        }
        
        // 搜索编解码器
        std::string video_codec_lower = media.video_codec;
        std::transform(video_codec_lower.begin(), video_codec_lower.end(), video_codec_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (video_codec_lower.find(query_lower) != std::string::npos) {
            results.push_back(media);
            continue;
        }
        
        // 搜索元数据
        for (const auto& [key, value] : media.metadata) {
            std::string value_lower = value;
            std::transform(value_lower.begin(), value_lower.end(), value_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (value_lower.find(query_lower) != std::string::npos) {
                results.push_back(media);
                break;
            }
        }
    }
    
    return results;
}

MediaInfo MediaManager::analyze_file(const std::string& filepath) {
    return analyzer_->analyze(filepath);
}

std::string MediaManager::get_supported_formats() const {
    return analyzer_->get_format_info();
}

std::vector<std::string> MediaManager::get_supported_codecs() const {
    return analyzer_->get_supported_codecs();
}

std::string MediaManager::generate_id() const {
    static std::atomic<int> counter{0};
    return "media_" + std::to_string(++counter);
}

bool MediaManager::is_media_file(const std::string& filename) const {
    // 使用FFmpeg的格式检测
    if (!analyzer_->is_supported_format(filename)) {
        return false;
    }
    
    // 额外检查常见扩展名
    static const std::vector<std::string> media_extensions = {
        ".mp4", ".mkv", ".avi", ".mov", ".flv", ".webm", ".wmv", ".mpg", ".mpeg", ".m4v",
        ".mp3", ".wav", ".flac", ".aac", ".ogg", ".m4a", ".wma", ".opus", ".mka",
        ".jpg", ".jpeg", ".png", ".gif", ".bmp", ".tiff", ".webp"
    };
    
    std::string ext = fs::path(filename).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    return std::find(media_extensions.begin(), media_extensions.end(), ext) != media_extensions.end();
}