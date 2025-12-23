#include "media_analyzer.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <vector>
#include <algorithm>

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>
#include <libavcodec/avcodec.h>
}

MediaAnalyzer::MediaAnalyzer() : format_ctx_(nullptr), initialized_(false) {
    avformat_network_init();
}

MediaAnalyzer::~MediaAnalyzer() {
    cleanup();
    avformat_network_deinit();
}

void MediaAnalyzer::cleanup() {
    if (format_ctx_) {
        avformat_close_input(&format_ctx_);
        format_ctx_ = nullptr;
    }
    initialized_ = false;
}

MediaInfo MediaAnalyzer::analyze(const std::string& filepath) {
    MediaInfo info;
    info.success = false;
    info.filename = filepath;
    
    // 初始化所有字段
    info.video_width = 0;
    info.video_height = 0;
    info.video_frame_rate = 0.0;
    info.audio_sample_rate = 0;
    info.audio_channels = 0;
    info.video_codec = "unknown";
    info.audio_codec = "unknown";
    info.duration = 0.0;
    info.bit_rate = 0;
    info.format_name = "unknown";
    info.format_long_name = "Unknown Format";
    
    cleanup();
    
    std::cout << "\n[ANALYZE] 开始分析: " << filepath << std::endl;
    
    // 1. 打开文件
    int ret = avformat_open_input(&format_ctx_, filepath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char error_buffer[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buffer, sizeof(error_buffer));
        info.error_message = std::string("打开文件失败: ") + error_buffer;
        std::cerr << "[ERROR] " << info.error_message << std::endl;
        return info;
    }
    
    // 2. 获取流信息
    ret = avformat_find_stream_info(format_ctx_, nullptr);
    if (ret < 0) {
        char error_buffer[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, error_buffer, sizeof(error_buffer));
        info.error_message = std::string("获取流信息失败: ") + error_buffer;
        std::cerr << "[ERROR] " << info.error_message << std::endl;
        cleanup();
        return info;
    }
    
    initialized_ = true;
    
    // 3. 基本信息
    info.format_name = format_ctx_->iformat->name ? format_ctx_->iformat->name : "unknown";
    info.format_long_name = format_ctx_->iformat->long_name ? format_ctx_->iformat->long_name : "Unknown Format";
    
    if (format_ctx_->duration != AV_NOPTS_VALUE) {
        info.duration = format_ctx_->duration / (double)AV_TIME_BASE;
    }
    info.bit_rate = format_ctx_->bit_rate;
    
    std::cout << "[ANALYZE] 格式: " << info.format_name << std::endl;
    std::cout << "[ANALYZE] 时长: " << std::fixed << std::setprecision(3) << info.duration << "s" << std::endl;
    std::cout << "[ANALYZE] 比特率: " << info.bit_rate << " bps" << std::endl;
    std::cout << "[ANALYZE] 流数量: " << format_ctx_->nb_streams << std::endl;
    
    // 4. 元数据
    if (format_ctx_->metadata) {
        AVDictionaryEntry* tag = nullptr;
        while ((tag = av_dict_get(format_ctx_->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            if (tag->key && tag->value) {
                info.metadata[tag->key] = tag->value;
            }
        }
    }
    
    // 5. 分析每个流
    int video_stream_index = -1;
    int audio_stream_index = -1;
    
    for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
        AVStream* stream = format_ctx_->streams[i];
        if (!stream || !stream->codecpar) {
            continue;
        }
        
        StreamInfo stream_info = analyze_stream(format_ctx_, stream, i);
        info.streams.push_back(stream_info);
        
        // 记录第一个视频流和音频流
        if (stream_info.codec_type == "video" && video_stream_index < 0) {
            video_stream_index = i;
            info.video_width = stream_info.width;
            info.video_height = stream_info.height;
            info.video_frame_rate = stream_info.frame_rate;
            info.video_codec = stream_info.codec_name;
        } else if (stream_info.codec_type == "audio" && audio_stream_index < 0) {
            audio_stream_index = i;
            info.audio_sample_rate = stream_info.sample_rate;
            info.audio_channels = stream_info.channels;
            info.audio_codec = stream_info.codec_name;
        }
    }
    
    info.success = true;
    cleanup();
    return info;
}

StreamInfo MediaAnalyzer::analyze_stream(AVFormatContext* format_ctx, AVStream* stream, int stream_index) {
    (void)format_ctx;
    StreamInfo info;
    info.index = stream_index;
    
    // 默认值
    info.codec_type = "unknown";
    info.codec_name = "unknown";
    info.codec_long_name = "Unknown Codec";
    info.bit_rate = 0;
    info.width = 0;
    info.height = 0;
    info.frame_rate = 0.0;
    info.pixel_format = "unknown";
    info.sample_rate = 0;
    info.channels = 0;
    info.channel_layout = "unknown";
    info.sample_format = "unknown";
    info.duration = 0.0;
    info.nb_frames = 0;
    
    if (!stream || !stream->codecpar) {
        return info;
    }
    
    AVCodecParameters* codecpar = stream->codecpar;
    
    // 流类型
    switch (codecpar->codec_type) {
        case AVMEDIA_TYPE_VIDEO: info.codec_type = "video"; break;
        case AVMEDIA_TYPE_AUDIO: info.codec_type = "audio"; break;
        case AVMEDIA_TYPE_SUBTITLE: info.codec_type = "subtitle"; break;
        case AVMEDIA_TYPE_DATA: info.codec_type = "data"; break;
        default: info.codec_type = "unknown";
    }
    
    // 获取编解码器信息 - 使用 avcodec_descriptor_get
    const AVCodecDescriptor* codec_desc = avcodec_descriptor_get(codecpar->codec_id);
    if (codec_desc) {
        info.codec_name = codec_desc->name ? codec_desc->name : "unknown";
        info.codec_long_name = codec_desc->long_name ? codec_desc->long_name : "Unknown Codec";
    } else {
        // 回退：根据 codec_id 判断常见编解码器
        switch (codecpar->codec_id) {
            case AV_CODEC_ID_H264: info.codec_name = "h264"; info.codec_long_name = "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10"; break;
            case AV_CODEC_ID_H265: info.codec_name = "h265"; info.codec_long_name = "H.265 / HEVC"; break;
            case AV_CODEC_ID_MPEG4: info.codec_name = "mpeg4"; info.codec_long_name = "MPEG-4 part 2"; break;
            case AV_CODEC_ID_VP9: info.codec_name = "vp9"; info.codec_long_name = "Google VP9"; break;
            case AV_CODEC_ID_AV1: info.codec_name = "av1"; info.codec_long_name = "Alliance for Open Media AV1"; break;
            
            // 音频编解码器
            case AV_CODEC_ID_AAC: info.codec_name = "aac"; info.codec_long_name = "AAC (Advanced Audio Coding)"; break;
            case AV_CODEC_ID_MP3: info.codec_name = "mp3"; info.codec_long_name = "MP3 (MPEG audio layer 3)"; break;
            case AV_CODEC_ID_AC3: info.codec_name = "ac3"; info.codec_long_name = "ATSC A/52A (AC-3)"; break;
            case AV_CODEC_ID_EAC3: info.codec_name = "eac3"; info.codec_long_name = "ATSC A/52B (AC-3, E-AC-3)"; break;
            case AV_CODEC_ID_FLAC: info.codec_name = "flac"; info.codec_long_name = "FLAC (Free Lossless Audio Codec)"; break;
            case AV_CODEC_ID_OPUS: info.codec_name = "opus"; info.codec_long_name = "Opus"; break;
            case AV_CODEC_ID_VORBIS: info.codec_name = "vorbis"; info.codec_long_name = "Vorbis"; break;
            
            default: 
                // 尝试使用 avcodec_find_decoder 作为最后手段
                const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
                if (codec) {
                    info.codec_name = codec->name ? codec->name : "unknown";
                    info.codec_long_name = codec->long_name ? codec->long_name : "Unknown Codec";
                }
                break;
        }
    }
    
    std::cout << "[STREAM] 流 #" << stream_index << ": 类型=" << info.codec_type 
              << ", codec_id=" << codecpar->codec_id 
              << ", 编解码器=" << info.codec_name << std::endl;
    
    info.bit_rate = codecpar->bit_rate;
    
    // 时长
    if (stream->duration != AV_NOPTS_VALUE && stream->time_base.den > 0) {
        info.duration = stream->duration * av_q2d(stream->time_base);
    }
    
    // 帧数
    if (stream->nb_frames > 0) {
        info.nb_frames = stream->nb_frames;
    }
    
    // 视频流
    if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        info.width = codecpar->width;
        info.height = codecpar->height;
        
        std::cout << "[VIDEO] 分辨率: " << info.width << "x" << info.height << std::endl;
        
        // 安全检查
        if (info.width <= 0 || info.width > 10000) info.width = 0;
        if (info.height <= 0 || info.height > 10000) info.height = 0;
        
        // 帧率
        if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
            info.frame_rate = av_q2d(stream->avg_frame_rate);
        } else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
            info.frame_rate = av_q2d(stream->r_frame_rate);
        }
        
        std::cout << "[VIDEO] 帧率: " << info.frame_rate << " fps" << std::endl;
        
        // 像素格式
        if (codecpar->format != AV_PIX_FMT_NONE) {
            const AVPixelFormat pix_fmt = static_cast<AVPixelFormat>(codecpar->format);
            const char* pix_fmt_name = av_get_pix_fmt_name(pix_fmt);
            if (pix_fmt_name) {
                info.pixel_format = pix_fmt_name;
            }
        }
    }
    
    // 音频流
    if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        info.sample_rate = codecpar->sample_rate;
        info.channels = codecpar->channels;
        
        std::cout << "[AUDIO] 采样率: " << info.sample_rate << " Hz, 声道: " << info.channels << std::endl;
        
        // 安全检查
        if (info.sample_rate <= 0 || info.sample_rate > 384000) info.sample_rate = 0;
        if (info.channels <= 0 || info.channels > 100) info.channels = 0;
        
        // 声道布局
        if (codecpar->channel_layout) {
            char layout_name[256] = {0};
            av_get_channel_layout_string(layout_name, sizeof(layout_name), 
                                        codecpar->channels, codecpar->channel_layout);
            info.channel_layout = layout_name;
        } else if (info.channels > 0) {
            switch (info.channels) {
                case 1: info.channel_layout = "mono"; break;
                case 2: info.channel_layout = "stereo"; break;
                case 6: info.channel_layout = "5.1"; break;
                case 8: info.channel_layout = "7.1"; break;
                default: info.channel_layout = std::to_string(info.channels) + " channels";
            }
        }
        
        std::cout << "[AUDIO] 声道布局: " << info.channel_layout << std::endl;
        
        // 采样格式
        if (codecpar->format != AV_SAMPLE_FMT_NONE) {
            const AVSampleFormat sample_fmt = static_cast<AVSampleFormat>(codecpar->format);
            const char* sample_fmt_name = av_get_sample_fmt_name(sample_fmt);
            if (sample_fmt_name) {
                info.sample_format = sample_fmt_name;
            } else {
                info.sample_format = get_sample_format_name(sample_fmt);
            }
        }
    }
    
    return info;
}

void MediaAnalyzer::extract_metadata(AVDictionary* metadata, std::map<std::string, std::string>& output) {
    AVDictionaryEntry* tag = nullptr;
    while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        if (tag->key && tag->value) {
            output[tag->key] = tag->value;
        }
    }
}

std::string MediaAnalyzer::get_codec_type(AVMediaType type) {
    switch (type) {
        case AVMEDIA_TYPE_VIDEO: return "video";
        case AVMEDIA_TYPE_AUDIO: return "audio";
        case AVMEDIA_TYPE_SUBTITLE: return "subtitle";
        case AVMEDIA_TYPE_DATA: return "data";
        case AVMEDIA_TYPE_ATTACHMENT: return "attachment";
        default: return "unknown";
    }
}

std::string MediaAnalyzer::get_sample_format_name(AVSampleFormat format) {
    switch (format) {
        case AV_SAMPLE_FMT_U8: return "u8";
        case AV_SAMPLE_FMT_S16: return "s16";
        case AV_SAMPLE_FMT_S32: return "s32";
        case AV_SAMPLE_FMT_FLT: return "flt";
        case AV_SAMPLE_FMT_DBL: return "dbl";
        case AV_SAMPLE_FMT_U8P: return "u8p";
        case AV_SAMPLE_FMT_S16P: return "s16p";
        case AV_SAMPLE_FMT_S32P: return "s32p";
        case AV_SAMPLE_FMT_FLTP: return "fltp";
        case AV_SAMPLE_FMT_DBLP: return "dblp";
        default: return "unknown";
    }
}

std::string MediaAnalyzer::get_channel_layout_name(uint64_t channel_layout, int channels) {
    if (channel_layout == 0) {
        // If no specific layout, return generic based on channel count
        switch (channels) {
            case 1: return "mono";
            case 2: return "stereo";
            case 6: return "5.1";
            case 8: return "7.1";
            default: return std::to_string(channels) + " channels";
        }
    }
    
    char layout_name[256];
    av_get_channel_layout_string(layout_name, sizeof(layout_name), channels, channel_layout);
    return layout_name;
}

std::string MediaAnalyzer::get_format_info() {
    std::stringstream ss;
    ss << "FFmpeg Version: " << av_version_info() << "\n";
    return ss.str();
}

std::vector<std::string> MediaAnalyzer::get_supported_codecs() {
    std::vector<std::string> codecs;
    
    const AVCodecDescriptor* desc = nullptr;
    while ((desc = avcodec_descriptor_next(desc))) {
        if (desc->name) {
            codecs.push_back(desc->name);
        }
    }
    
    // 移除重复项并排序
    std::sort(codecs.begin(), codecs.end());
    codecs.erase(std::unique(codecs.begin(), codecs.end()), codecs.end());
    
    return codecs;
}

bool MediaAnalyzer::is_supported_format(const std::string& filename) {
    // Get file extension
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) return false;
    
    std::string ext = filename.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Common media extensions
    static const std::vector<std::string> supported_extensions = {
        "mp4", "mkv", "avi", "mov", "flv", "webm", "wmv", "mpg", "mpeg",
        "mp3", "wav", "flac", "aac", "ogg", "m4a", "wma", "opus"
    };
    
    return std::find(supported_extensions.begin(), supported_extensions.end(), ext) != supported_extensions.end();
}