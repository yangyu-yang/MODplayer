#ifndef SIMPLE_SERVER_H
#define SIMPLE_SERVER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>

class SimpleServer {
public:
    struct Route {
        std::string method;
        std::string path;
        std::function<std::string(const std::string&)> handler;
    };
    
    SimpleServer(int port = 8080);
    ~SimpleServer();
    
    bool start();
    void stop();
    bool is_running() const;
    
    void get(const std::string& path, std::function<std::string(const std::string&)> handler);
    
private:
    void run();
    
    int port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::vector<Route> routes_;
};

#endif // SIMPLE_SERVER_H