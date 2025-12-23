#ifndef HLS_PROCESSOR_H
#define HLS_PROCESSOR_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

// 前向声明
namespace std {
    class thread;
}

struct HLSStreamStatus {
    std::string stream_id;
    std::string media_id;
    std::string status; // "creating", "ready", "error", "stopped"
    std::string error_message;
    int segments_generated = 0;
    int total_segments = 0;
    double progress = 0.0;
    int viewers = 0;
    
    std::map<std::string, std::string> to_json() const;
};

struct HLSStreamConfig {
    std::string stream_id;
    std::string media_path;
    std::string output_dir;
    std::string media_id;
    std::string playlist_path;
    std::string segment_prefix;
    
    int video_bitrate = 2000;
    int audio_bitrate = 128;
    int segment_duration = 4;
    int max_segments = 10;
    std::string resolution = "1920x1080";
    std::string video_codec = "h264";
    std::string audio_codec = "aac";
    
    // 实时转码相关配置
    bool realtime_transcode = true;
    int buffer_size = 10; // 缓冲区大小（分片数）
};

class HLSProcessor {
public:
    static HLSProcessor& get_instance();
    
    HLSProcessor(const HLSProcessor&) = delete;
    HLSProcessor& operator=(const HLSProcessor&) = delete;
    
    HLSProcessor();
    ~HLSProcessor();
    
    // 创建流（支持实时转码）
    bool create_stream(const std::string& media_path, 
                      const std::string& media_id,
                      const HLSStreamConfig& config = HLSStreamConfig());
    
    // 获取流状态
    HLSStreamStatus get_stream_status(const std::string& stream_id) const;
    
    // 获取播放列表
    std::string get_playlist(const std::string& stream_id) const;
    
    // 获取分片文件
    std::vector<char> get_segment(const std::string& stream_id, 
                                 const std::string& segment_name) const;
    
    // 列出所有流
    std::vector<std::string> list_streams() const;
    
    // 停止流
    bool stop_stream(const std::string& stream_id);
    
    // 实时转码相关方法
    bool start_realtime_transcode(const std::string& stream_id, 
                                 const std::string& media_path,
                                 const HLSStreamConfig& config);
    
    void stop_realtime_transcode(const std::string& stream_id);
    
    // 检查流是否存在
    bool stream_exists(const std::string& stream_id) const;
    
private:
    // 实时转码线程函数
    void transcode_thread_function(const std::string& stream_id, 
                                  const std::string& media_path,
                                  const HLSStreamConfig& config);
    
    // 读取播放列表文件
    std::string read_playlist_file(const std::string& stream_id) const;
    
    // 读取分片文件
    std::vector<char> read_segment_file(const std::string& stream_id, 
                                      const std::string& segment_name) const;
    
    // 清理过期分片
    void cleanup_old_segments(const std::string& stream_id, int keep_count = 10);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

#endif // HLS_PROCESSOR_H