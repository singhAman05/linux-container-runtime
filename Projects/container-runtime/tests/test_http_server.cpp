#include <gtest/gtest.h>
#include "http_server.h"
#include "container.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>

using namespace runtime;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string http_get(uint16_t port, const std::string& path) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return "";

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd); return "";
    }

    std::string req = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
    send(fd, req.data(), req.size(), 0);

    std::string resp;
    resp.resize(4096);
    ssize_t n = recv(fd, resp.data(), resp.size() - 1, 0);
    resp.resize(n > 0 ? n : 0);
    close(fd);
    return resp;
}

static std::string http_post(uint16_t port, const std::string& path,
                              const std::string& body) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return "";

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(fd); return "";
    }

    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: localhost\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "\r\n"
        << body;
    std::string r = req.str();
    send(fd, r.data(), r.size(), 0);

    std::string resp;
    resp.resize(4096);
    ssize_t n = recv(fd, resp.data(), resp.size() - 1, 0);
    resp.resize(n > 0 ? n : 0);
    close(fd);
    return resp;
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class HttpServerTest : public ::testing::Test {
protected:
    static constexpr uint16_t PORT = 19876; // unlikely to conflict

    ContainerManager mgr;
    std::unique_ptr<HttpServer> server;

    void SetUp() override {
        server = std::make_unique<HttpServer>(mgr, PORT);
        server->start();
        // Give the accept thread a moment to bind.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        server->stop();
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(HttpServerTest, HealthzReturns200) {
    std::string resp = http_get(PORT, "/healthz");
    EXPECT_NE(resp.find("200 OK"), std::string::npos);
    EXPECT_NE(resp.find("ok"), std::string::npos);
}

TEST_F(HttpServerTest, ListContainersEmpty) {
    std::string resp = http_get(PORT, "/containers");
    EXPECT_NE(resp.find("200 OK"), std::string::npos);
    EXPECT_NE(resp.find("[]"), std::string::npos);
}

TEST_F(HttpServerTest, CreateContainerReturns201) {
    std::string body = R"({"command":["/bin/true"]})";
    std::string resp = http_post(PORT, "/containers", body);
    EXPECT_NE(resp.find("201 Created"), std::string::npos);
    EXPECT_NE(resp.find("created"), std::string::npos);
}

TEST_F(HttpServerTest, CreateThenListShowsContainer) {
    std::string body = R"({"command":["/bin/true"]})";
    http_post(PORT, "/containers", body);

    std::string list = http_get(PORT, "/containers");
    EXPECT_NE(list.find("/bin/true"), std::string::npos);
}

TEST_F(HttpServerTest, UnknownRouteReturns404) {
    std::string resp = http_get(PORT, "/nonexistent");
    EXPECT_NE(resp.find("404"), std::string::npos);
}

TEST_F(HttpServerTest, ServerIsRunning) {
    EXPECT_TRUE(server->is_running());
}
