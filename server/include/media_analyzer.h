#ifndef MEDIA_ANALYZER_H
#define MEDIA_ANALYZER_H

#include <string>
#include <map>
#include <vector>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
}

struct StreamInfo {
    int index;
    std::string codec_type;  // "video", "audio", "subtitle"
    std::string codec_name;
    std::string codec_long_name;
    int bit_rate;
    int width;
    int height;
    double frame_rate;
    std::string pixel_format;
    int sample_rate;
    int channels;
    std::string channel_layout;
    std::string sample_format;
    double duration;
    int64_t nb_frames;
};

struct MediaInfo {
    std::string filename;
    std::string format_name;
    std::string format_long_name;
    double duration;
    int64_t size;
    int bit_rate;
    std::map<std::string, std::string> metadata;
    std::vector<StreamInfo> streams;
    std::string creation_time;
    
    // Video-specific (from first video stream)
    int video_width;
    int video_height;
    double video_frame_rate;
    std::string video_codec;
    
    // Audio-specific (from first audio stream)
    int audio_sample_rate;
    int audio_channels;
    std::string audio_codec;
    
    // Error information
    bool success;
    std::string error_message;
};

class MediaAnalyzer {
public:
    MediaAnalyzer();
    ~MediaAnalyzer();
    
    // Analyze media file and extract detailed information
    MediaInfo analyze(const std::string& filepath);
    
    // Get media format information
    static std::string get_format_info();
    
    // Get supported codecs
    static std::vector<std::string> get_supported_codecs();
    
    // Check if file is supported media format
    static bool is_supported_format(const std::string& filename);
    
private:
    void cleanup();
    void extract_metadata(AVDictionary* metadata, std::map<std::string, std::string>& output);
    StreamInfo analyze_stream(AVFormatContext* format_ctx, AVStream* stream, int stream_index);
    std::string get_codec_type(AVMediaType type);
    std::string get_sample_format_name(AVSampleFormat format);
    std::string get_channel_layout_name(uint64_t channel_layout, int channels);
    
    AVFormatContext* format_ctx_;
    bool initialized_;
};

#endif // MEDIA_ANALYZER_H