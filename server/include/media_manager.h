#ifndef MEDIA_MANAGER_H
#define MEDIA_MANAGER_H

#include "media_analyzer.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>

struct MediaFile {
    std::string id;
    std::string filename;
    std::string path;
    uint64_t size;
    double duration;
    std::string format;
    std::string video_codec;
    std::string audio_codec;
    int width;
    int height;
    double frame_rate;
    int bitrate;
    int audio_sample_rate;
    int audio_channels;
    std::string channel_layout;
    std::string created_time;
    
    // Extended info from FFmpeg
    std::map<std::string, std::string> metadata;
    std::vector<StreamInfo> streams;
    
    // JSON serialization
    std::map<std::string, std::string> to_json() const;
    
    // Create from MediaInfo
    static MediaFile from_media_info(const MediaInfo& info, const std::string& filename, const std::string& path, uint64_t size);
};

class MediaManager {
public:
    static MediaManager& get_instance();
    
    // Scan media directory with FFmpeg analysis
    bool scan_directory(const std::string& path);
    
    // Get all media files
    std::vector<MediaFile> get_all_media() const;
    
    // Get specific media file
    MediaFile* get_media(const std::string& id);
    MediaFile* get_media_by_name(const std::string& filename);
    
    // Search media files
    std::vector<MediaFile> search(const std::string& query) const;
    
    // Analyze single file with FFmpeg
    MediaInfo analyze_file(const std::string& filepath);
    
    // Get supported formats and codecs
    std::string get_supported_formats() const;
    std::vector<std::string> get_supported_codecs() const;
    
private:
    MediaManager();
    ~MediaManager() = default;
    
    mutable std::mutex mutex_;
    std::vector<MediaFile> media_files_;
    std::map<std::string, MediaFile*> media_map_;
    std::unique_ptr<MediaAnalyzer> analyzer_;
    
    // Helper functions
    std::string generate_id() const;
    bool is_media_file(const std::string& filename) const;
};

#endif // MEDIA_MANAGER_H