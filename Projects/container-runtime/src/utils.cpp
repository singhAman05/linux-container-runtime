#include "utils.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <random>
#include <sys/stat.h>
#include <sys/types.h>
#include <filesystem>
#include <ctime>

namespace runtime {

void write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::trunc);
    if (!f) throw_errno("write_file: open " + path);
    f << content;
    if (!f) throw_errno("write_file: write " + path);
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw_errno("read_file: open " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool dir_exists(const std::string& path) {
    struct stat st{};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void mkdir_p(const std::string& path) {
    std::filesystem::create_directories(path);
}

void rmdir_r(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) throw std::runtime_error("rmdir_r " + path + ": " + ec.message());
}

std::string random_hex(size_t len) {
    static std::mt19937_64 rng{std::random_device{}()};
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len);
    std::uniform_int_distribution<int> dist(0, 15);
    for (size_t i = 0; i < len; ++i)
        out += hex[dist(rng)];
    return out;
}

static std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&t, &tm_buf);
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

void log_info(const std::string& msg) {
    std::cout << "[INFO]  " << timestamp() << " " << msg << "\n";
}

void log_warn(const std::string& msg) {
    std::cout << "[WARN]  " << timestamp() << " " << msg << "\n";
}

void log_error(const std::string& msg) {
    std::cerr << "[ERROR] " << timestamp() << " " << msg << "\n";
}

} // namespace runtime
