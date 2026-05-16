#include "cgroup.h"
#include "utils.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace runtime {

CgroupManager::CgroupManager(const std::string& container_id)
    : container_id_(container_id)
    , cgroup_path_(std::string(CGROUP_ROOT) + "/" + container_id)
{}

CgroupManager::~CgroupManager() {
    // Do not remove on destructor — lifecycle is managed explicitly via remove().
}

// Check that the cgroup v2 unified hierarchy is mounted.
bool CgroupManager::cgroup_v2_available() const {
    struct stat st{};
    // cgroup v2 unified hierarchy is a cgroup2 filesystem
    if (stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0)
        return true;
    // Fallback: check if the root cgroup dir exists
    return dir_exists("/sys/fs/cgroup");
}

void CgroupManager::apply(const ResourceLimits& limits) {
    if (!cgroup_v2_available()) {
        log_warn("cgroup v2 not available — skipping resource limits");
        return;
    }

    // Create the cgroup directory hierarchy.
    mkdir_p(cgroup_path_);

    // ── Memory limit ────────────────────────────────────────────────────────
    // memory.max: hard limit in bytes (process receives SIGKILL if exceeded)
    write_file(cgroup_path_ + "/memory.max",
               std::to_string(limits.memory_limit_bytes));

    // memory.swap.max: disable swap for containers (security + predictability)
    write_file(cgroup_path_ + "/memory.swap.max", "0");

    // ── CPU quota ───────────────────────────────────────────────────────────
    // cpu.max: "<quota> <period>" in microseconds
    // e.g. "50000 100000" = 50% of one CPU core
    write_file(cgroup_path_ + "/cpu.max",
               std::to_string(limits.cpu_quota_us) + " " +
               std::to_string(limits.cpu_period_us));

    // ── PID limit ───────────────────────────────────────────────────────────
    // pids.max: prevents fork bombs inside the container
    write_file(cgroup_path_ + "/pids.max",
               std::to_string(limits.pids_max));

    log_info("cgroup applied: " + cgroup_path_);
}

void CgroupManager::add_pid(pid_t pid) {
    if (!dir_exists(cgroup_path_)) {
        log_warn("cgroup not set up — cannot add PID " + std::to_string(pid));
        return;
    }
    // Writing to cgroup.procs moves the process (and its threads) into this cgroup.
    write_file(cgroup_path_ + "/cgroup.procs", std::to_string(pid));
    log_info("PID " + std::to_string(pid) + " joined cgroup " + cgroup_path_);
}

void CgroupManager::remove() {
    if (!dir_exists(cgroup_path_)) return;
    // cgroup directory can only be removed when it has no live processes.
    // rmdir (not recursive) is the correct call for cgroup dirs.
    if (::rmdir(cgroup_path_.c_str()) != 0) {
        log_warn("rmdir cgroup " + cgroup_path_ + ": " + std::strerror(errno));
    }
}

void CgroupManager::write_file(const std::string& filename,
                                const std::string& value) {
    runtime::write_file(filename, value);
}

} // namespace runtime
