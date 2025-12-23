#ifndef SIMPLE_ROUTES_H
#define SIMPLE_ROUTES_H

#include "server.h"

// Setup all routes
void setup_routes(SimpleServer& server);

// Utility functions
std::string create_json_response(const std::string& message, bool success = true);

#endif // SIMPLE_ROUTES_H