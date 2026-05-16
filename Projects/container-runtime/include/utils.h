#pragma once

#include <string>
#include <stdexcept>
#include <cerrno>
#include <cstring>

namespace runtime {

// Throw a std::runtime_error with message + strerror(errno).
inline void throw_errno(const std::string& msg) {
    throw std::runtime_error(msg + ": " + std::strerror(errno));
}

// Write entire string to a file (truncating). Throws on error.
void write_file(const std::string& path, const std::string& content);

// Read entire file into string. Throws on error.
std::string read_file(const std::string& path);

// Check if a directory exists.
bool dir_exists(const std::string& path);

// Create directory (and parents). Throws on error.
void mkdir_p(const std::string& path);

// Remove directory recursively. Throws on error.
void rmdir_r(const std::string& path);

// Generate a random hex string of `len` characters.
std::string random_hex(size_t len);

// Simple structured log line: [LEVEL] msg
void log_info (const std::string& msg);
void log_warn (const std::string& msg);
void log_error(const std::string& msg);

} // namespace runtime
