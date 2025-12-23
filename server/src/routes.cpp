#include "routes.h"
#include "media_manager.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>
#include <map>
#include <functional>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// Helper function to read file content
std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Get MIME type for file extension
std::string get_mime_type(const std::string& path) {
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }
    
    std::string ext = path.substr(dot_pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    static const std::map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"txt", "text/plain"},
        {"mp4", "video/mp4"},
        {"mkv", "video/x-matroska"},
        {"avi", "video/x-msvideo"},
        {"mov", "video/quicktime"},
        {"mp3", "audio/mpeg"},
        {"wav", "audio/wav"},
        {"flac", "audio/flac"}
    };
    
    auto it = mime_types.find(ext);
    return it != mime_types.end() ? it->second : "application/octet-stream";
}

// Static file handler
std::string serve_static_file(const std::string& request_path) {
    // Clean the path
    std::string clean_path = request_path;
    if (clean_path == "/") {
        clean_path = "/index.html";
    }
    
    // Remove query parameters
    size_t query_pos = clean_path.find('?');
    if (query_pos != std::string::npos) {
        clean_path = clean_path.substr(0, query_pos);
    }
    
    // Security: prevent directory traversal
    if (clean_path.find("..") != std::string::npos) {
        return "HTTP/1.1 403 Forbidden\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n"
               "\r\n"
               "Forbidden";
    }
    
    // Map to web directory
    std::string file_path = "../web" + clean_path;
    
    // Default to index.html for directories
    if (fs::is_directory(file_path)) {
        file_path += "/index.html";
    }
    
    // Check if file exists
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        return "HTTP/1.1 404 Not Found\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n"
               "\r\n"
               "File not found: " + clean_path;
    }
    
    // Read file content
    std::string content = read_file(file_path);
    if (content.empty()) {
        return "HTTP/1.1 500 Internal Server Error\r\n"
               "Content-Type: text/plain\r\n"
               "Connection: close\r\n"
               "\r\n"
               "Error reading file";
    }
    
    // Get MIME type
    std::string mime_type = get_mime_type(file_path);
    
    // Build response
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: " << mime_type << "\r\n"
             << "Connection: close\r\n"
             << "\r\n"
             << content;
    
    return response.str();
}

std::string create_json_response(const std::string& message, bool success) {
    std::stringstream ss;
    ss << "HTTP/1.1 " << (success ? "200 OK" : "400 Bad Request") << "\r\n"
       << "Content-Type: application/json\r\n"
       << "Connection: close\r\n"
       << "\r\n"
       << "{"
       << "\"success\": " << (success ? "true" : "false") << ", "
       << "\"message\": \"" << message << "\", "
       << "\"timestamp\": " << std::chrono::duration_cast<std::chrono::seconds>(
           std::chrono::system_clock::now().time_since_epoch()).count()
       << "}";
    return ss.str();
}

void setup_routes(SimpleServer& server) {
    std::cout << "Setting up routes..." << std::endl;
    
    // Initialize media manager
    auto& media_mgr = MediaManager::get_instance();
    media_mgr.scan_directory("../media");
    
    // 1. 首先注册API路由，避免被静态文件路由拦截
    
    // Server status
    server.get("/api/status", [](const std::string&) -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        std::stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/json\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << "{"
           << "\"status\": \"running\", "
           << "\"time\": \"" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "\", "
           << "\"uptime\": 0, "
           << "\"version\": \"1.0.0\""
           << "}";
        return ss.str();
    });
    
    // Media list
    server.get("/api/media/list", [](const std::string&) -> std::string {
        auto& media_mgr = MediaManager::get_instance();
        auto media_files = media_mgr.get_all_media();
        
        std::stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/json\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << "{\"media_files\": [";
        
        for (size_t i = 0; i < media_files.size(); ++i) {
            const auto& file = media_files[i];
            auto json_data = file.to_json();
            
            ss << "{";
            bool first = true;
            for (const auto& [key, value] : json_data) {
                if (!first) ss << ", ";
                ss << "\"" << key << "\": ";
                
                // Check if value is numeric
                bool is_numeric = !value.empty() && std::all_of(value.begin(), value.end(), 
                    [](char c) { return std::isdigit(c) || c == '.'; });
                
                if (is_numeric) {
                    ss << value;
                } else {
                    ss << "\"" << value << "\"";
                }
                first = false;
            }
            ss << "}";
            
            if (i < media_files.size() - 1) {
                ss << ",";
            }
        }
        
        ss << "], \"count\": " << media_files.size() << "}";
        return ss.str();
    });
    
    // Rescan media directory
    server.get("/api/media/scan", [](const std::string&) -> std::string {
        auto& media_mgr = MediaManager::get_instance();
        bool success = media_mgr.scan_directory("../media");
        
        std::stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/json\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << "{"
           << "\"success\": " << (success ? "true" : "false") << ", "
           << "\"message\": \"" << (success ? "Media directory scanned successfully" : "Failed to scan media directory") << "\", "
           << "\"path\": \"../media\""
           << "}";
        return ss.str();
    });
    
    // Get specific media info
    server.get("/api/media/:id", [](const std::string& request) -> std::string {
        // 解析请求行获取路径
        std::istringstream request_stream(request);
        std::string method, full_path, version;
        request_stream >> method >> full_path >> version;
        
        // 提取路径和参数
        size_t query_pos = full_path.find('?');
        std::string path = (query_pos != std::string::npos) ? 
                          full_path.substr(0, query_pos) : full_path;
        
        // 从路径中提取 ID
        // 路径格式: /api/media/{id}
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t end = path.find('/', start);
            if (end == std::string::npos) {
                parts.push_back(path.substr(start));
                break;
            }
            parts.push_back(path.substr(start, end - start));
            start = end + 1;
        }
        
        std::string media_id = "";
        if (parts.size() >= 3) {
            media_id = parts[2];  // /api/media/{id}
        }
        
        auto& media_mgr = MediaManager::get_instance();
        auto* media = media_mgr.get_media(media_id);
        
        std::stringstream ss;
        ss << "HTTP/1.1 " << (media ? "200 OK" : "404 Not Found") << "\r\n"
           << "Content-Type: application/json\r\n"
           << "Connection: close\r\n"
           << "\r\n";
        
        if (media) {
            auto json_data = media->to_json();
            ss << "{";
            bool first = true;
            for (const auto& [key, value] : json_data) {
                if (!first) ss << ", ";
                ss << "\"" << key << "\": \"" << value << "\"";
                first = false;
            }
            ss << "}";
        } else {
            ss << "{\"error\": \"Media not found\", \"requested_id\": \"" << media_id << "\"}";
        }
        
        return ss.str();
    });
    
    // Create session
    server.get("/api/session/create", [](const std::string& request) -> std::string {
        // Parse query parameters
        std::string media_id = "1";
        std::string filename = "";
        
        size_t query_start = request.find('?');
        if (query_start != std::string::npos) {
            std::string query = request.substr(query_start + 1);
            
            // Simple query parsing
            size_t param_start = 0;
            while (param_start < query.length()) {
                size_t param_end = query.find('&', param_start);
                std::string param = (param_end == std::string::npos) ? 
                                   query.substr(param_start) : 
                                   query.substr(param_start, param_end - param_start);
                
                size_t equal_pos = param.find('=');
                if (equal_pos != std::string::npos) {
                    std::string key = param.substr(0, equal_pos);
                    std::string value = param.substr(equal_pos + 1);
                    
                    if (key == "media_id") media_id = value;
                    else if (key == "filename") filename = value;
                }
                
                param_start = (param_end == std::string::npos) ? query.length() : param_end + 1;
            }
        }
        
        std::stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
           << "Content-Type: application/json\r\n"
           << "Connection: close\r\n"
           << "\r\n"
           << "{"
           << "\"success\": true, "
           << "\"session_id\": \"session_" << media_id << "\", "
           << "\"media_id\": \"" << media_id << "\", "
           << "\"filename\": \"" << filename << "\", "
           << "\"status\": \"created\", "
           << "\"stream_url\": \"http://localhost:8080/stream/session_" << media_id << "\""
           << "}";
        return ss.str();
    });
    
    // 2. 然后注册静态文件路由
    server.get("/", [](const std::string&) -> std::string {
        return serve_static_file("/");
    });
    
    server.get("/index.html", [](const std::string&) -> std::string {
        return serve_static_file("/");
    });
    
    // CSS files
    server.get("/css/:filename", [](const std::string& request) -> std::string {
        // Extract filename from request
        std::istringstream request_stream(request);
        std::string method, full_path, version;
        request_stream >> method >> full_path >> version;
        
        return serve_static_file(full_path);
    });
    
    // JS files
    server.get("/js/:filename", [](const std::string& request) -> std::string {
        std::istringstream request_stream(request);
        std::string method, full_path, version;
        request_stream >> method >> full_path >> version;
        
        return serve_static_file(full_path);
    });
    
    // Images
    server.get("/images/:filename", [](const std::string& request) -> std::string {
        std::istringstream request_stream(request);
        std::string method, full_path, version;
        request_stream >> method >> full_path >> version;
        
        return serve_static_file(full_path);
    });
    
    std::cout << "Routes setup completed" << std::endl;
    std::cout << "Web directory: ../web" << std::endl;
    std::cout << "Media directory: ../media" << std::endl;
}