#pragma once

#include "types.h"
#include <string>
#include <sys/types.h>

namespace runtime {

// cgroup v2 unified hierarchy is mounted at /sys/fs/cgroup.
// We create a sub-cgroup at /sys/fs/cgroup/container-runtime/<id>/
// and write resource limits into the appropriate interface files.

constexpr const char* CGROUP_ROOT = "/sys/fs/cgroup/container-runtime";

class CgroupManager {
public:
    explicit CgroupManager(const std::string& container_id);
    ~CgroupManager();

    // Create the cgroup directory and write resource limits.
    void apply(const ResourceLimits& limits);

    // Move a PID into this cgroup.
    void add_pid(pid_t pid);

    // Remove the cgroup directory (called on destroy).
    void remove();

    std::string path() const { return cgroup_path_; }

private:
    std::string container_id_;
    std::string cgroup_path_;

    void write_file(const std::string& filename, const std::string& value);
    bool cgroup_v2_available() const;
};

} // namespace runtime
