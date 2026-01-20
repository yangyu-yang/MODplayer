#ifndef SIMPLE_SERVER_H
#define SIMPLE_SERVER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <sys/epoll.h>

class SimpleServer {
public:
    struct Route {
        std::string method;
        std::string path;
        std::function<std::string(const std::string&)> handler;
    };

    // HTTP 请求解析结果
    struct HttpRequest {
        std::string method;
        std::string path;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;
        std::map<std::string, std::string> query_params;
    };

    SimpleServer(int port = 8080);
    ~SimpleServer();

    bool start();
    void stop();
    bool is_running() const;

    // HTTP 方法路由注册
    void get(const std::string& path, std::function<std::string(const std::string&)> handler);
    void post(const std::string& path, std::function<std::string(const std::string&)> handler);
    void put(const std::string& path, std::function<std::string(const std::string&)> handler);
    void del(const std::string& path, std::function<std::string(const std::string&)> handler);

    // 通用路由注册
    void add_route(const std::string& method, const std::string& path,
                   std::function<std::string(const std::string&)> handler);

private:
    void run();
    void setup_server_socket();
    void cleanup();

    // HTTP 解析相关
    HttpRequest parse_http_request(const std::string& request) const;
    std::string extract_clean_path(const std::string& path) const;
    std::map<std::string, std::string> parse_query_params(const std::string& query_string) const;
    bool path_matches(const std::string& request_path, const std::string& route_path,
                      std::map<std::string, std::string>& params) const;

    // 客户端连接处理
    void handle_client_connection(int client_fd);
    void send_response(int client_fd, const std::string& response);

    // 工具函数
    static bool set_socket_nonblocking(int fd);
    static std::vector<std::string> split_string(const std::string& str, char delimiter);

    int port_;
    int server_fd_;
    int epoll_fd_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::vector<Route> routes_;

    // 常量定义
    static const int MAX_EVENTS = 64;
    static const int BUFFER_SIZE = 4096;
    static const int BACKLOG = 1024;
};

#endif // SIMPLE_SERVER_H