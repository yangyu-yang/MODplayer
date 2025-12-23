#include "server.h"
#include "routes.h"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> running{true};

void signal_handler(int signal) {
	(void)signal;
    std::cout << "Received signal, shutting down..." << std::endl;
    running = false;
}

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "Simple Media Server v1.0" << std::endl;
    std::cout << "======================================" << std::endl;
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // Create server instance
        SimpleServer server(8080);
        
        // Setup routes
        setup_routes(server);
        
        // Start server
        if (server.start()) {
            std::cout << "Server started on port 8080" << std::endl;
            std::cout << "Test endpoints:" << std::endl;
            std::cout << "  - http://localhost:8080/" << std::endl;
            std::cout << "  - http://localhost:8080/api/status" << std::endl;
            std::cout << "  - http://localhost:8080/api/media/list" << std::endl;
            std::cout << "Press Ctrl+C to stop" << std::endl;
            
            // Keep running
            while (running && server.is_running()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            server.stop();
        } else {
            std::cerr << "Failed to start server" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Server stopped" << std::endl;
    return 0;
}