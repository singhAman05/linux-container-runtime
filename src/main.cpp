#include "container.h"
#include "http_server.h"
#include "utils.h"

#include <iostream>
#include <csignal>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <sys/wait.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

using namespace runtime;
using json = nlohmann::json;

// Global server pointer for signal handler.
static HttpServer* g_server = nullptr;

static void signal_handler(int sig) {
    log_info("received signal " + std::to_string(sig) + " — shutting down");
    if (g_server) g_server->stop();
}

static void print_usage(const char* prog) {
    std::cerr
        << "Usage:\n"
        << "  " << prog << " server [--port PORT]      Start REST API server\n"
        << "  " << prog << " run <image_rootfs> <cmd>  Run a container immediately\n"
        << "\n"
        << "Examples:\n"
        << "  " << prog << " server --port 8080\n"
        << "  " << prog << " run /path/to/rootfs /bin/sh\n";
}

// ── `run` subcommand: create, start, wait for a single container ──────────────
static int cmd_run(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Usage: run <rootfs> <command> [args...]\n";
        return 1;
    }

    ContainerSpec spec;
    spec.rootfs  = args[0];
    spec.command = std::vector<std::string>(args.begin() + 1, args.end());
    spec.limits.memory_limit_bytes = 128 * 1024 * 1024; // 128 MiB

    ContainerManager mgr;
    std::string id = mgr.create(spec);
    log_info("created container: " + id);

    mgr.start(id);
    log_info("container running, waiting for exit…");

    // Wait for the container to finish.
    ContainerInfo info = mgr.get(id);
    int wstatus = 0;
    waitpid(info.init_pid, &wstatus, 0);
    int exit_code = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;

    log_info("container exited with code " + std::to_string(exit_code));
    mgr.stop(id);
    mgr.destroy(id);
    return exit_code;
}

// ── `server` subcommand: start REST API + block ───────────────────────────────
static int cmd_server(uint16_t port) {
    ContainerManager mgr;
    HttpServer       server(mgr, port);

    g_server = &server;
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    server.start();
    log_info("container-runtime server started (pid=" +
             std::to_string(getpid()) + ")");

    // Block until the server is stopped by a signal.
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    std::string subcommand = argv[1];

    if (subcommand == "server") {
        uint16_t port = 8080;
        for (int i = 2; i < argc - 1; ++i) {
            if (std::string(argv[i]) == "--port")
                port = static_cast<uint16_t>(std::stoi(argv[i + 1]));
        }
        return cmd_server(port);
    }

    if (subcommand == "run") {
        std::vector<std::string> args;
        for (int i = 2; i < argc; ++i)
            args.emplace_back(argv[i]);
        return cmd_run(args);
    }

    print_usage(argv[0]);
    return 1;
}
