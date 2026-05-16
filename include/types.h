#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace runtime {

// ── Container state machine ───────────────────────────────────────────────────
enum class ContainerState {
    Created,
    Running,
    Stopped,
    Destroyed
};

inline std::string state_to_string(ContainerState s) {
    switch (s) {
        case ContainerState::Created:   return "created";
        case ContainerState::Running:   return "running";
        case ContainerState::Stopped:   return "stopped";
        case ContainerState::Destroyed: return "destroyed";
    }
    return "unknown";
}

// ── Resource limits passed to cgroup v2 ──────────────────────────────────────
struct ResourceLimits {
    uint64_t memory_limit_bytes = 64 * 1024 * 1024;  // 64 MiB default
    uint64_t cpu_quota_us       = 50000;              // 50ms per 100ms period
    uint64_t cpu_period_us      = 100000;
    uint64_t pids_max           = 32;
};

// ── Full container specification ─────────────────────────────────────────────
struct ContainerSpec {
    std::string              id;
    std::string              hostname;
    std::string              rootfs;           // path to rootfs directory
    std::vector<std::string> command;          // argv[0..n]
    ResourceLimits           limits;
    bool                     enable_seccomp = true;
    bool                     enable_network = false; // network NS isolation
};

// ── Runtime info about a live container ──────────────────────────────────────
struct ContainerInfo {
    ContainerSpec  spec;
    ContainerState state = ContainerState::Created;
    pid_t          init_pid = -1;
    int            exit_code = -1;
};

} // namespace runtime
