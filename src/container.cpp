#include "container.h"
#include "namespace.h"
#include "seccomp_filter.h"
#include "utils.h"

#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace runtime {

// Stack size for the cloned child process.
constexpr size_t CLONE_STACK_SIZE = 8 * 1024 * 1024; // 8 MiB

// Data passed from parent to the child function via clone().
struct CloneArgs {
    const ContainerSpec* spec;
};

// Entry point for the cloned container init process.
// This runs inside the new namespaces.
static int container_init(void* arg) {
    auto* ca = static_cast<CloneArgs*>(arg);
    const ContainerSpec& spec = *ca->spec;

    try {
        // ── 1. Finish namespace setup (hostname, rootfs, /proc) ─────────────
        setup_namespaces(spec);

        // ── 2. Install seccomp-bpf allowlist ────────────────────────────────
        if (spec.enable_seccomp)
            install_seccomp_filter();

        // ── 3. exec() the container workload ────────────────────────────────
        if (spec.command.empty())
            throw std::runtime_error("container command is empty");

        std::vector<const char*> argv;
        argv.reserve(spec.command.size() + 1);
        for (auto& s : spec.command)
            argv.push_back(s.c_str());
        argv.push_back(nullptr);

        execvp(argv[0], const_cast<char* const*>(argv.data()));
        // If execvp returns, it failed.
        throw_errno("execvp " + spec.command[0]);

    } catch (const std::exception& e) {
        log_error(std::string("container_init: ") + e.what());
        return 1;
    }
    return 0; // unreachable
}

// ── ContainerManager ──────────────────────────────────────────────────────────

ContainerManager::~ContainerManager() {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& [id, info] : containers_) {
        if (info.state == ContainerState::Running && info.init_pid > 0) {
            kill(info.init_pid, SIGKILL);
        }
    }
}

std::string ContainerManager::generate_id() {
    return runtime::random_hex(12);
}

std::string ContainerManager::create(ContainerSpec spec) {
    if (spec.id.empty())
        spec.id = generate_id();

    if (spec.hostname.empty())
        spec.hostname = "container-" + spec.id.substr(0, 6);

    std::lock_guard<std::mutex> lock(mu_);
    if (containers_.count(spec.id))
        throw std::runtime_error("container already exists: " + spec.id);

    ContainerInfo info;
    info.spec  = std::move(spec);
    info.state = ContainerState::Created;

    std::string id = info.spec.id;
    containers_[id] = std::move(info);
    log_info("container created: " + id);
    return id;
}

void ContainerManager::start(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = containers_.find(id);
    if (it == containers_.end())
        throw std::out_of_range("no such container: " + id);

    ContainerInfo& info = it->second;
    if (info.state != ContainerState::Created)
        throw std::runtime_error("container not in Created state: " + id);

    // ── Apply cgroup resource limits before spawning ─────────────────────────
    CgroupManager cg(id);
    cg.apply(info.spec.limits);

    // ── Spawn the container process ──────────────────────────────────────────
    pid_t pid = spawn(info.spec);
    if (pid < 0)
        throw std::runtime_error("failed to spawn container process");

    // Move the new child into our cgroup.
    cg.add_pid(pid);

    info.init_pid = pid;
    info.state    = ContainerState::Running;

    log_info("container started: " + id + " (pid=" + std::to_string(pid) + ")");
}

void ContainerManager::stop(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = containers_.find(id);
    if (it == containers_.end())
        throw std::out_of_range("no such container: " + id);

    ContainerInfo& info = it->second;
    if (info.state != ContainerState::Running)
        throw std::runtime_error("container not running: " + id);

    // Send SIGKILL to the container init process.
    if (info.init_pid > 0) {
        if (kill(info.init_pid, SIGKILL) != 0 && errno != ESRCH)
            throw_errno("kill container init");

        int wstatus = 0;
        waitpid(info.init_pid, &wstatus, 0);
        info.exit_code = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : -1;
    }

    info.state = ContainerState::Stopped;
    log_info("container stopped: " + id);
}

void ContainerManager::destroy(const std::string& id) {
    std::lock_guard<std::mutex> lock(mu_);

    auto it = containers_.find(id);
    if (it == containers_.end())
        throw std::out_of_range("no such container: " + id);

    ContainerInfo& info = it->second;
    if (info.state == ContainerState::Running)
        throw std::runtime_error("stop container before destroying: " + id);

    // Clean up cgroup.
    CgroupManager cg(id);
    cg.remove();

    containers_.erase(it);
    log_info("container destroyed: " + id);
}

std::vector<ContainerInfo> ContainerManager::list() const {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<ContainerInfo> result;
    result.reserve(containers_.size());
    for (auto& [_, info] : containers_)
        result.push_back(info);
    return result;
}

ContainerInfo ContainerManager::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mu_);
    return containers_.at(id); // throws std::out_of_range if missing
}

pid_t ContainerManager::spawn(const ContainerSpec& spec) {
    // Allocate a stack for the child process.
    std::vector<char> stack(CLONE_STACK_SIZE);
    char* stack_top = stack.data() + CLONE_STACK_SIZE; // stack grows downward

    CloneArgs args{&spec};

    int flags = namespace_flags(spec) | SIGCHLD;

    pid_t pid = clone(container_init, stack_top, flags, &args);
    if (pid < 0)
        throw_errno("clone");

    return pid;
}

} // namespace runtime
