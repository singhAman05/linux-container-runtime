#include "http_server.h"
#include "utils.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace runtime {

// ── JSON serialization helpers ────────────────────────────────────────────────
static json spec_to_json(const ContainerSpec& spec) {
    json j;
    j["id"]       = spec.id;
    j["hostname"] = spec.hostname;
    j["rootfs"]   = spec.rootfs;
    j["command"]  = spec.command;
    j["seccomp"]  = spec.enable_seccomp;
    j["network"]  = spec.enable_network;
    j["limits"]["memory_bytes"] = spec.limits.memory_limit_bytes;
    j["limits"]["cpu_quota_us"] = spec.limits.cpu_quota_us;
    j["limits"]["cpu_period_us"]= spec.limits.cpu_period_us;
    j["limits"]["pids_max"]     = spec.limits.pids_max;
    return j;
}

static json info_to_json(const ContainerInfo& info) {
    json j;
    j["id"]        = info.spec.id;
    j["hostname"]  = info.spec.hostname;
    j["state"]     = state_to_string(info.state);
    j["init_pid"]  = info.init_pid;
    j["exit_code"] = info.exit_code;
    j["spec"]      = spec_to_json(info.spec);
    return j;
}

static ContainerSpec spec_from_json(const json& j) {
    ContainerSpec spec;
    spec.id       = j.value("id", "");
    spec.hostname = j.value("hostname", "");
    spec.rootfs   = j.value("rootfs", "");
    spec.command  = j.value("command", std::vector<std::string>{});
    spec.enable_seccomp = j.value("seccomp", true);
    spec.enable_network = j.value("network", false);
    if (j.contains("limits")) {
        auto& lim = j["limits"];
        spec.limits.memory_limit_bytes = lim.value("memory_bytes", 64ULL * 1024 * 1024);
        spec.limits.cpu_quota_us       = lim.value("cpu_quota_us", 50000ULL);
        spec.limits.cpu_period_us      = lim.value("cpu_period_us", 100000ULL);
        spec.limits.pids_max           = lim.value("pids_max", 32ULL);
    }
    return spec;
}

// ── HTTP response builders ────────────────────────────────────────────────────
std::string HttpServer::http_response(int status, const std::string& content_type,
                                       const std::string& body) {
    std::string status_text = "OK";
    if      (status == 201) status_text = "Created";
    else if (status == 400) status_text = "Bad Request";
    else if (status == 404) status_text = "Not Found";
    else if (status == 409) status_text = "Conflict";
    else if (status == 500) status_text = "Internal Server Error";

    std::ostringstream resp;
    resp << "HTTP/1.1 " << status << " " << status_text << "\r\n"
         << "Content-Type: " << content_type << "\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n"
         << "\r\n"
         << body;
    return resp.str();
}

std::string HttpServer::ok_json(const std::string& body) {
    return http_response(200, "application/json", body);
}

std::string HttpServer::err_json(int status, const std::string& msg) {
    json j;
    j["error"] = msg;
    return http_response(status, "application/json", j.dump());
}

// ── Route registration ────────────────────────────────────────────────────────
void HttpServer::register_routes() {
    // GET /healthz
    routes_.push_back({"GET", "/healthz", [this](const std::string&, const std::string&) {
        return http_response(200, "application/json",
                             nlohmann::json{{"status","ok"}}.dump());
    }});

    // POST /containers  — create
    routes_.push_back({"POST", "/containers",
        [this](const std::string& body, const std::string&) -> std::string {
            try {
                auto j    = json::parse(body);
                auto spec = spec_from_json(j);
                auto id   = mgr_.create(std::move(spec));
                json resp;
                resp["id"] = id;
                resp["status"] = "created";
                return http_response(201, "application/json", resp.dump());
            } catch (const std::exception& e) {
                return err_json(400, e.what());
            }
        }
    });

    // GET /containers — list
    routes_.push_back({"GET", "/containers",
        [this](const std::string&, const std::string&) -> std::string {
            auto list = mgr_.list();
            json arr = json::array();
            for (auto& c : list) arr.push_back(info_to_json(c));
            return ok_json(arr.dump());
        }
    });

    // GET /containers/:id — inspect
    routes_.push_back({"GET", "/containers/",
        [this](const std::string&, const std::string& id) -> std::string {
            try {
                return ok_json(info_to_json(mgr_.get(id)).dump());
            } catch (const std::out_of_range&) {
                return err_json(404, "container not found: " + id);
            }
        }
    });

    // POST /containers/:id/start
    routes_.push_back({"POST", "/containers//start",
        [this](const std::string&, const std::string& id) -> std::string {
            try {
                mgr_.start(id);
                return ok_json(json{{"status","started"}}.dump());
            } catch (const std::out_of_range&) {
                return err_json(404, "container not found: " + id);
            } catch (const std::exception& e) {
                return err_json(500, e.what());
            }
        }
    });

    // POST /containers/:id/stop
    routes_.push_back({"POST", "/containers//stop",
        [this](const std::string&, const std::string& id) -> std::string {
            try {
                mgr_.stop(id);
                return ok_json(json{{"status","stopped"}}.dump());
            } catch (const std::out_of_range&) {
                return err_json(404, "container not found: " + id);
            } catch (const std::exception& e) {
                return err_json(500, e.what());
            }
        }
    });

    // DELETE /containers/:id — destroy
    routes_.push_back({"DELETE", "/containers/",
        [this](const std::string&, const std::string& id) -> std::string {
            try {
                mgr_.destroy(id);
                return ok_json(json{{"status","destroyed"}}.dump());
            } catch (const std::out_of_range&) {
                return err_json(404, "container not found: " + id);
            } catch (const std::exception& e) {
                return err_json(409, e.what());
            }
        }
    });
}

// ── Dispatcher ────────────────────────────────────────────────────────────────
std::string HttpServer::dispatch(const std::string& method,
                                  const std::string& path,
                                  const std::string& body) {
    // Exact match first (e.g. GET /containers, POST /containers, GET /healthz)
    for (auto& route : routes_) {
        if (route.method == method && route.path == path)
            return route.handler(body, "");
    }

    // Parameterised: /containers/:id  or  /containers/:id/start|stop
    if (path.rfind("/containers/", 0) == 0) {
        std::string rest = path.substr(12); // after "/containers/"

        // DELETE /containers/:id or GET /containers/:id
        if (rest.find('/') == std::string::npos) {
            for (auto& route : routes_) {
                if (route.method == method && route.path == "/containers/")
                    return route.handler(body, rest);
            }
        }

        // POST /containers/:id/start  or  /containers/:id/stop
        auto slash = rest.find('/');
        if (slash != std::string::npos) {
            std::string id     = rest.substr(0, slash);
            std::string action = rest.substr(slash + 1);
            std::string route_path = "/containers//" + action;
            for (auto& route : routes_) {
                if (route.method == method && route.path == route_path)
                    return route.handler(body, id);
            }
        }
    }

    return err_json(404, "route not found: " + method + " " + path);
}

// ── Client handler ────────────────────────────────────────────────────────────
void HttpServer::handle_client(int client_fd) {
    // Read request (simple: read up to 64KB)
    std::string request;
    request.resize(65536);
    ssize_t n = recv(client_fd, request.data(), request.size() - 1, 0);
    if (n <= 0) { close(client_fd); return; }
    request.resize(n);

    // Parse request line: "METHOD /path HTTP/1.x\r\n"
    std::istringstream ss(request);
    std::string method, path, version;
    ss >> method >> path >> version;

    // Extract body (after \r\n\r\n)
    std::string body;
    auto sep = request.find("\r\n\r\n");
    if (sep != std::string::npos)
        body = request.substr(sep + 4);

    std::string response = dispatch(method, path, body);

    send(client_fd, response.data(), response.size(), MSG_NOSIGNAL);
    close(client_fd);
}

// ── Accept loop ───────────────────────────────────────────────────────────────
void HttpServer::accept_loop() {
    while (running_.load()) {
        struct sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);

        int client_fd = accept(server_fd_,
                               reinterpret_cast<sockaddr*>(&client_addr),
                               &addr_len);
        if (client_fd < 0) {
            if (running_.load())
                log_warn("accept() failed: " + std::string(strerror(errno)));
            continue;
        }

        // Handle each client in a detached thread.
        std::thread([this, client_fd]() {
            handle_client(client_fd);
        }).detach();
    }
}

// ── Constructor / start / stop ────────────────────────────────────────────────
HttpServer::HttpServer(ContainerManager& mgr, uint16_t port)
    : mgr_(mgr), port_(port) {
    register_routes();
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0)
        throw_errno("socket");

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        throw_errno("bind port " + std::to_string(port_));

    if (listen(server_fd_, 32) < 0)
        throw_errno("listen");

    running_.store(true);
    accept_thread_ = std::thread(&HttpServer::accept_loop, this);

    log_info("REST API listening on port " + std::to_string(port_));
}

void HttpServer::stop() {
    if (!running_.exchange(false)) return;
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    if (accept_thread_.joinable())
        accept_thread_.join();
    log_info("HTTP server stopped");
}

} // namespace runtime
