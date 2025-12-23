#include "server.h"
#include <iostream>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <vector>
#include <string>

// 辅助函数：分割路径
std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = 0;
    
    while ((end = path.find('/', start)) != std::string::npos) {
        if (end != start) {  // 避免空段
            result.push_back(path.substr(start, end - start));
        }
        start = end + 1;
    }
    
    if (start < path.length()) {
        result.push_back(path.substr(start));
    }
    
    return result;
}

// 辅助函数：比较路径是否匹配
bool path_matches(const std::string& request_path, const std::string& route_path) {
    // 处理参数化路由（如 /api/media/:id）
    auto request_parts = split_path(request_path);
    auto route_parts = split_path(route_path);
    
    if (request_parts.size() != route_parts.size()) {
        return false;
    }
    
    for (size_t i = 0; i < route_parts.size(); ++i) {
        if (route_parts[i][0] == ':') {
            // 参数化部分，匹配任何值
            continue;
        }
        if (route_parts[i] != request_parts[i]) {
            return false;
        }
    }
    
    return true;
}

SimpleServer::SimpleServer(int port) : port_(port) {
    std::cout << "Server created, port: " << port_ << std::endl;
}

SimpleServer::~SimpleServer() {
    stop();
}

bool SimpleServer::start() {
    if (running_) {
        return false;
    }
    
    running_ = true;
    server_thread_ = std::thread(&SimpleServer::run, this);
    
    return true;
}

void SimpleServer::stop() {
    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

bool SimpleServer::is_running() const {
    return running_;
}

void SimpleServer::get(const std::string& path, std::function<std::string(const std::string&)> handler) {
    routes_.push_back({"GET", path, handler});
    std::cout << "Route registered: GET " << path << std::endl;
}

void SimpleServer::run() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket creation failed" << std::endl;
        return;
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        std::cerr << "setsockopt failed" << std::endl;
        close(server_fd);
        return;
    }
    
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed on port: " << port_ << std::endl;
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Listen failed" << std::endl;
        close(server_fd);
        return;
    }
    
    std::cout << "Server ready, waiting for connections..." << std::endl;
    
    while (running_) {
        // Non-blocking accept
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        
        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            std::cerr << "Select error" << std::endl;
            break;
        }
        
        if (activity > 0 && FD_ISSET(server_fd, &readfds)) {
            sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
            
            if (client_fd >= 0) {
                char buffer[1024] = {0};
                ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    std::string request(buffer);
                    std::string response = "HTTP/1.1 404 Not Found\r\n"
                                           "Content-Type: application/json\r\n"
                                           "Connection: close\r\n"
                                           "\r\n"
                                           "{\"error\": \"Not found\"}";
                    
                    // 解析请求行
                    std::istringstream request_stream(request);
                    std::string method, path, version;
                    request_stream >> method >> path >> version;
                    
                    // 去除查询参数
                    size_t query_pos = path.find('?');
                    std::string clean_path = (query_pos != std::string::npos) ? 
                                             path.substr(0, query_pos) : path;
                    
                    // 查找匹配的路由
                    for (const auto& route : routes_) {
                        if (route.method == method && path_matches(clean_path, route.path)) {
                            response = route.handler(request);
                            break;
                        }
                    }
                    
                    ssize_t bytes_written = write(client_fd, response.c_str(), response.length());
					(void)bytes_written;
                }
                
                close(client_fd);
            }
        }
    }
    
    close(server_fd);
    std::cout << "Server thread stopped" << std::endl;
}