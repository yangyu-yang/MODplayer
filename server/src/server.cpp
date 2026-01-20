#include "server.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <system_error>

// 常量定义
const int SimpleServer::MAX_EVENTS;
const int SimpleServer::BUFFER_SIZE;
const int SimpleServer::BACKLOG;

SimpleServer::SimpleServer(int port) : port_(port), server_fd_(-1), epoll_fd_(-1) {
    std::cout << "服务器创建，端口: " << port_ << std::endl;
}

SimpleServer::~SimpleServer() {
    stop();
    cleanup();
}

bool SimpleServer::start() {
    if (running_) {
        std::cerr << "服务器已在运行中" << std::endl;
        return false;
    }

    try {
        setup_server_socket();
    } catch (const std::exception& e) {
        std::cerr << "服务器启动失败: " << e.what() << std::endl;
        cleanup();
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&SimpleServer::run, this);

    std::cout << "服务器启动成功，监听端口: " << port_ << std::endl;
    return true;
}

void SimpleServer::stop() {
    if (running_) {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        std::cout << "服务器已停止" << std::endl;
    }
}

bool SimpleServer::is_running() const {
    return running_;
}

void SimpleServer::get(const std::string& path, std::function<std::string(const std::string&)> handler) {
    add_route("GET", path, handler);
}

void SimpleServer::post(const std::string& path, std::function<std::string(const std::string&)> handler) {
    add_route("POST", path, handler);
}

void SimpleServer::put(const std::string& path, std::function<std::string(const std::string&)> handler) {
    add_route("PUT", path, handler);
}

void SimpleServer::del(const std::string& path, std::function<std::string(const std::string&)> handler) {
    add_route("DELETE", path, handler);
}

void SimpleServer::add_route(const std::string& method, const std::string& path,
                            std::function<std::string(const std::string&)> handler) {
    routes_.push_back({method, path, handler});
    std::cout << "路由注册: " << method << " " << path << std::endl;
}

void SimpleServer::setup_server_socket() {
    // 创建 socket
    server_fd_ = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd_ < 0) {
        throw std::system_error(errno, std::system_category(), "socket 创建失败");
    }

    // 设置 SO_REUSEADDR
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::system_error(errno, std::system_category(), "setsockopt 失败");
    }

    // 绑定地址
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(server_fd_);
        throw std::system_error(errno, std::system_category(), "bind 失败，端口: " + std::to_string(port_));
    }

    // 监听
    if (listen(server_fd_, BACKLOG) < 0) {
        close(server_fd_);
        throw std::system_error(errno, std::system_category(), "listen 失败");
    }

    // 创建 epoll 实例
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        close(server_fd_);
        throw std::system_error(errno, std::system_category(), "epoll_create1 失败");
    }

    // 添加服务器 socket 到 epoll
    epoll_event event{};
    event.events = EPOLLIN | EPOLLET;  // 边缘触发模式
    event.data.fd = server_fd_;

    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, server_fd_, &event) < 0) {
        close(epoll_fd_);
        close(server_fd_);
        throw std::system_error(errno, std::system_category(), "epoll_ctl 失败");
    }
}

void SimpleServer::run() {
    epoll_event events[MAX_EVENTS];

    std::cout << "服务器主循环开始" << std::endl;

    while (running_) {
        int num_events = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);  // 1秒超时
        
        if (num_events < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断，继续循环
            }
            std::cerr << "epoll_wait 错误: " << strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < num_events; ++i) {
            int event_fd = events[i].data.fd;
            uint32_t event_flags = events[i].events;

            // 处理错误事件
            if (event_flags & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
                if (event_fd != server_fd_) {
                    close(event_fd);
                }
                continue;
            }

            if (event_fd == server_fd_) {
                // 接受新连接（边缘触发，需要循环接受）
                while (running_) {
                    sockaddr_in client_addr{};
                    socklen_t addr_len = sizeof(client_addr);

                    int client_fd = accept4(server_fd_, reinterpret_cast<sockaddr*>(&client_addr),
                                          &addr_len, SOCK_NONBLOCK);

                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;  // 没有更多连接
                        }
                        std::cerr << "accept 错误: " << strerror(errno) << std::endl;
                        break;
                    }

                    // 添加客户端到 epoll
                    epoll_event client_event{};
                    client_event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
                    client_event.data.fd = client_fd;

                    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
                        std::cerr << "PS_FAILED: " << strerror(errno) << std::endl;
                        close(client_fd);
                        continue;
                    }

                    // 记录连接信息
                    char client_ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
                    std::cout << "新连接: " << client_ip << ":" << ntohs(client_addr.sin_port)
                              << " (fd=" << client_fd << ")" << std::endl;
                }
            } else {
                // 处理客户端数据
                handle_client_connection(event_fd);
            }
        }
    }

    std::cout << "服务器主循环结束" << std::endl;
}

void SimpleServer::handle_client_connection(int client_fd) {
    std::string request_data;
    char buffer[BUFFER_SIZE];

    // 边缘触发模式，需要循环读取直到没有数据
    while (true) {
        ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            request_data.append(buffer, bytes_read);

            // 检查是否收到完整的 HTTP 请求（以 \r\n\r\n 结束）
            if (request_data.find("\r\n\r\n") != std::string::npos) {
                break;  // 收到完整请求
            }
        } else if (bytes_read == 0) {
            // 连接关闭
            close(client_fd);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;  // 没有更多数据可读
            } else {
                std::cerr << "recv 错误 (fd=" << client_fd << "): " << strerror(errno) << std::endl;
                close(client_fd);
                return;
            }
        }
    }

    if (request_data.empty()) {
        return;
    }

    try {
        // 解析 HTTP 请求
        HttpRequest request = parse_http_request(request_data);

        // 查找匹配的路由
        std::string response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: application/json\r\n"
                               "Connection: close\r\n"
                               "Content-Length: 24\r\n"
                               "\r\n"
                               "{\"error\": \"Not found\"}";

        std::map<std::string, std::string> route_params;

        for (const auto& route : routes_) {
            if (route.method == request.method &&
                path_matches(request.path, route.path, route_params)) {
                response = route.handler(request_data);
                break;
            }
        }

        // 发送响应
        send_response(client_fd, response);

    } catch (const std::exception& e) {
        std::cerr << "请求处理错误: " << e.what() << std::endl;
        std::string error_response = "HTTP/1.1 500 Internal Server Error\r\n"
                                     "Content-Type: text/plain\r\n"
                                     "Connection: close\r\n"
                                     "\r\n"
                                     "Internal Server Error";
        send_response(client_fd, error_response);
    }

    // 短连接：处理完立即关闭
    close(client_fd);
}

SimpleServer::HttpRequest SimpleServer::parse_http_request(const std::string& request) const {
    HttpRequest result;
    std::istringstream stream(request);
    std::string line;

    // 解析请求行
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        line_stream >> result.method >> result.path >> result.version;

        // 去除 \r（如果存在）
        if (!result.version.empty() && result.version.back() == '\r') {
            result.version.pop_back();
        }
    }

    // 分离查询参数
    size_t query_pos = result.path.find('?');
    if (query_pos != std::string::npos) {
        std::string query_string = result.path.substr(query_pos + 1);
        result.query_params = parse_query_params(query_string);
        result.path = result.path.substr(0, query_pos);
    }

    // 解析头部
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // 去除首尾空白字符
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // 去除 \r（如果存在）
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }

            result.headers[key] = value;
        }
    }

    // 解析消息体（如果有）
    if (result.headers.count("Content-Length")) {
        try {
            size_t content_length = std::stoul(result.headers.at("Content-Length"));
            size_t header_end = request.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                header_end += 4;  // 跳过空行
                if (header_end < request.length()) {
                    result.body = request.substr(header_end, content_length);
                }
            }
        } catch (const std::exception&) {
            // 忽略 Content-Length 解析错误
        }
    }

    return result;
}

std::string SimpleServer::extract_clean_path(const std::string& path) const {
    size_t query_pos = path.find('?');
    return (query_pos != std::string::npos) ? path.substr(0, query_pos) : path;
}

std::map<std::string, std::string> SimpleServer::parse_query_params(const std::string& query_string) const {
    std::map<std::string, std::string> params;
    std::vector<std::string> pairs = split_string(query_string, '&');

    for (const auto& pair : pairs) {
        size_t equal_pos = pair.find('=');
        if (equal_pos != std::string::npos) {
            std::string key = pair.substr(0, equal_pos);
            std::string value = pair.substr(equal_pos + 1);
            // 这里可以添加 URL 解码
            params[key] = value;
        }
    }

    return params;
}

bool SimpleServer::path_matches(const std::string& request_path, const std::string& route_path,
                               std::map<std::string, std::string>& params) const {
    std::vector<std::string> request_parts = split_string(request_path, '/');
    std::vector<std::string> route_parts = split_string(route_path, '/');

    if (request_parts.size() != route_parts.size()) {
        return false;
    }
    
    for (size_t i = 0; i < route_parts.size(); ++i) {
        if (route_parts[i].empty()) continue;

        if (route_parts[i][0] == ':') {
            // 参数化路由部分
            std::string param_name = route_parts[i].substr(1);
            params[param_name] = request_parts[i];
        } else if (route_parts[i] != request_parts[i]) {
            return false;
        }
    }

    return true;
}

void SimpleServer::send_response(int client_fd, const std::string& response) {
    ssize_t total_sent = 0;
    size_t remaining = response.length();

    while (remaining > 0) {
        ssize_t sent = send(client_fd, response.c_str() + total_sent, remaining, MSG_NOSIGNAL);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 暂时不可写，可以重试或等待
                continue;
            }
            std::cerr << "send 错误 (fd=" << client_fd << "): " << strerror(errno) << std::endl;
            break;
        }

        total_sent += sent;
        remaining -= sent;
    }
}

bool SimpleServer::set_socket_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

std::vector<std::string> SimpleServer::split_string(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = 0;

    while ((end = str.find(delimiter, start)) != std::string::npos) {
        if (end != start) {
            result.push_back(str.substr(start, end - start));
        }
        start = end + 1;
    }

    if (start < str.length()) {
        result.push_back(str.substr(start));
    }

    return result;
}

void SimpleServer::cleanup() {
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
        epoll_fd_ = -1;
    }

    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
}