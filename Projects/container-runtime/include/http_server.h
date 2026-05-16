#pragma once

#include "container.h"
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <netinet/in.h>

namespace runtime {

// Route descriptor
struct Route {
    std::string method;   // "GET", "POST", "DELETE"
    std::string path;     // exact match, e.g. "/containers"
    std::function<std::string(const std::string& body, const std::string& param)> handler;
};

// Minimal HTTP/1.1 server built on raw POSIX sockets.
// Serves a REST API for the container lifecycle:
//
//   POST   /containers          – create (body: ContainerSpec JSON)
//   GET    /containers          – list all
//   GET    /containers/:id      – inspect one
//   POST   /containers/:id/start
//   POST   /containers/:id/stop
//   DELETE /containers/:id      – destroy
//   GET    /healthz             – liveness probe
//
class HttpServer {
public:
    HttpServer(ContainerManager& mgr, uint16_t port = 8080);
    ~HttpServer();

    // Start listening in a background thread.
    void start();

    // Graceful shutdown.
    void stop();

    bool is_running() const { return running_.load(); }
    uint16_t port() const   { return port_; }

private:
    ContainerManager& mgr_;
    uint16_t          port_;
    int               server_fd_ = -1;
    std::thread       accept_thread_;
    std::atomic<bool> running_{false};

    void accept_loop();
    void handle_client(int client_fd);

    // Parse "METHOD /path HTTP/1.x" and route to the right handler.
    std::string dispatch(const std::string& method,
                         const std::string& path,
                         const std::string& body);

    // Register route handlers.
    void register_routes();
    std::vector<Route> routes_;

    // JSON helpers
    std::string ok_json(const std::string& body);
    std::string err_json(int status, const std::string& msg);
    std::string http_response(int status, const std::string& content_type,
                               const std::string& body);
};

} // namespace runtime
