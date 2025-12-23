// server/include/ffmpeg_transcoder.h
#ifndef FFMPEG_TRANSCODER_H
#define FFMPEG_TRANSCODER_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

struct TranscodeConfig {
    std::string input_path;
    std::string output_dir;
    std::string stream_id;
    
    int video_bitrate = 2000;  // kbps
    int audio_bitrate = 128;   // kbps
    int segment_duration = 4;  // seconds
    int max_segments = 10;
    std::string resolution = "1920x1080";
    std::string video_codec = "libx264";
    std::string audio_codec = "aac";
    
    bool enable_logging = true;
};

class FFmpegTranscoder {
public:
    FFmpegTranscoder(const TranscodeConfig& config);
    ~FFmpegTranscoder();
    
    bool start();
    void stop();
    bool is_running() const;
    std::string get_status() const;
    int get_segment_count() const;
    
    std::string get_playlist() const;
    std::vector<char> get_segment(const std::string& segment_name) const;
    
private:
    void transcode_process();
    void cleanup();
    bool create_output_directory();
    std::string build_ffmpeg_command() const;
    
    TranscodeConfig config_;
    std::atomic<bool> is_running_{false};
    std::thread transcode_thread_;
    pid_t ffmpeg_pid_{-1};
    mutable std::mutex status_mutex_;
    std::string error_message_;
    int segment_count_{0};
};

#endif // FFMPEG_TRANSCODER_H