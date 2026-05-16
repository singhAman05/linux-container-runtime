#pragma once

#include "types.h"
#include "cgroup.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace runtime {

// ContainerManager is the central object that owns all container lifecycle.
// It is thread-safe: the REST API server calls into it from multiple threads.
class ContainerManager {
public:
    ContainerManager() = default;
    ~ContainerManager();

    // Create a container from spec; returns its ID.
    // State transitions: (none) → Created
    std::string create(ContainerSpec spec);

    // Start a previously created container.
    // State transitions: Created → Running
    void start(const std::string& id);

    // Stop a running container by sending SIGKILL to its init PID.
    // State transitions: Running → Stopped
    void stop(const std::string& id);

    // Destroy a stopped or created container; cleans up cgroup and state.
    // State transitions: Stopped/Created → Destroyed (then removed from map)
    void destroy(const std::string& id);

    // List all known containers.
    std::vector<ContainerInfo> list() const;

    // Get a single container by id. Throws std::out_of_range if not found.
    ContainerInfo get(const std::string& id) const;

private:
    mutable std::mutex                                    mu_;
    std::unordered_map<std::string, ContainerInfo>        containers_;

    // Spawn the container process using clone(2).
    pid_t spawn(const ContainerSpec& spec);

    // Generate a unique container ID.
    static std::string generate_id();
};

} // namespace runtime
