# linux-container-runtime

A minimal OCI-inspired Linux container runtime written in C++17, built to demonstrate core virtualization and isolation primitives used by production systems like [Firecracker](https://github.com/firecracker-microvm/firecracker).

## What It Does

Runs processes in lightweight isolated environments using Linux kernel primitives:

| Primitive | Purpose |
|-----------|---------|
| **PID namespace** | Container sees itself as PID 1; cannot observe host processes |
| **Mount namespace** | Independent filesystem view; can pivot into a custom rootfs |
| **UTS namespace** | Container has its own hostname |
| **IPC namespace** | Isolated System V IPC and POSIX message queues |
| **NET namespace** | Optional: fully isolated network stack |
| **cgroups v2** | Memory cap, CPU quota, PID limit — prevents resource exhaustion / fork bombs |
| **seccomp-bpf** | Default-deny syscall allowlist: 100+ permitted, dangerous calls (ptrace, mount, kexec_load, bpf) blocked with SCMP_ACT_KILL_PROCESS |

A REST API (built on raw POSIX sockets, no third-party HTTP library) provides the container lifecycle control plane.

## Architecture

```
┌─────────────────────────────────────────┐
│           REST API (port 8080)          │  ← HttpServer (raw POSIX sockets)
│  POST /containers                       │
│  GET  /containers/:id                   │
│  POST /containers/:id/start             │
│  POST /containers/:id/stop              │
│  DELETE /containers/:id                 │
└──────────────────┬──────────────────────┘
                   │
          ┌────────▼────────┐
          │ ContainerManager │  ← thread-safe lifecycle manager
          └────────┬─────────┘
                   │  clone(2) with CLONE_NEWPID|CLONE_NEWNS|CLONE_NEWUTS|CLONE_NEWIPC
          ┌────────▼─────────────────────────────┐
          │       Container Init Process         │
          │  1. setup_namespaces()               │  ← namespace.cpp
          │     - sethostname (UTS)              │
          │     - chroot into rootfs (MNT)       │
          │     - mount /proc                    │
          │  2. install_seccomp_filter()         │  ← seccomp_filter.cpp
          │     - libseccomp allowlist           │
          │     - SCMP_ACT_KILL_PROCESS default  │
          │  3. execvp(command)                  │
          └──────────────────────────────────────┘
                   │
          ┌────────▼─────────────────────────────┐
          │         CgroupManager                │  ← cgroup.cpp
          │  /sys/fs/cgroup/container-runtime/   │
          │  ├── memory.max                      │
          │  ├── memory.swap.max = 0             │
          │  ├── cpu.max = quota period          │
          │  └── pids.max                        │
          └──────────────────────────────────────┘
```

## Build

**Requirements:** Linux kernel ≥ 5.0, GCC/Clang with C++17, CMake ≥ 3.16, libseccomp-dev, nlohmann-json-dev

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install cmake g++ libseccomp-dev nlohmann-json3-dev

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Run

```bash
# Start the REST API server
sudo ./build/container-runtime server --port 8080

# OR: run a single container directly
sudo ./build/container-runtime run /path/to/rootfs /bin/sh
```

> **Note:** Namespace and cgroup operations require root (or `CAP_SYS_ADMIN`).

## REST API

```bash
# Create a container
curl -X POST http://localhost:8080/containers \
  -H 'Content-Type: application/json' \
  -d '{
    "command": ["/bin/sh", "-c", "echo hello from container"],
    "hostname": "mybox",
    "seccomp": true,
    "limits": {
      "memory_bytes": 134217728,
      "cpu_quota_us": 50000,
      "cpu_period_us": 100000,
      "pids_max": 32
    }
  }'
# → {"id":"a3f1c9...","status":"created"}

# Start it
curl -X POST http://localhost:8080/containers/a3f1c9.../start

# Inspect
curl http://localhost:8080/containers/a3f1c9...

# Stop and destroy
curl -X POST   http://localhost:8080/containers/a3f1c9.../stop
curl -X DELETE http://localhost:8080/containers/a3f1c9...

# Health check
curl http://localhost:8080/healthz
```

## Tests

```bash
cd build && ctest --output-on-failure
# or directly:
./runtime_tests
```

24 test cases across 7 suites covering: namespace flags, cgroup path construction,
seccomp filter installation, container lifecycle state machine, REST API routing and HTTP responses.

## Security Model

The seccomp-bpf filter follows a **default-deny** policy. Permitted syscall categories:

- Process lifecycle: `fork`, `clone`, `execve`, `wait4`, signals
- Memory: `mmap`, `munmap`, `mprotect`, `brk`
- File I/O: `read`, `write`, `open`, `close`, `stat`, `fcntl`
- Networking: `socket`, `connect`, `accept`, `send`, `recv`
- Polling: `epoll_*`, `poll`, `select`

Intentionally **blocked** (triggers `SCMP_ACT_KILL_PROCESS`):
- `ptrace` — no process tracing across containers
- `mount` — containers cannot remount filesystems
- `pivot_root` — reserved for the runtime itself
- `kexec_load` — cannot replace the running kernel
- `init_module` / `finit_module` — no kernel module loading
- `bpf` — containers cannot install BPF programs
- `perf_event_open` — side-channel mitigation

This mirrors the approach used by Firecracker's jailer and OCI runtimes (runc/containerd).

## Relation to Firecracker

Firecracker uses KVM to create microVMs rather than namespaces, but the fundamental
isolation goals are identical: multi-tenant workloads that cannot escape their
boundaries, with minimal overhead and a small attack surface. This project implements
the namespace/cgroup layer that sits *above* the VMM in a complete container stack,
and studies the same seccomp filtering philosophy used by Firecracker's jailer process.

## File Structure

```
.
├── CMakeLists.txt
├── include/
│   ├── types.h          # ContainerSpec, ContainerInfo, ResourceLimits
│   ├── namespace.h      # Linux namespace setup
│   ├── cgroup.h         # cgroups v2 resource limits
│   ├── seccomp_filter.h # seccomp-bpf allowlist
│   ├── container.h      # ContainerManager lifecycle
│   ├── http_server.h    # REST API server
│   └── utils.h          # logging, file I/O, helpers
├── src/
│   ├── main.cpp
│   ├── namespace.cpp
│   ├── cgroup.cpp
│   ├── seccomp_filter.cpp
│   ├── container.cpp
│   ├── http_server.cpp
│   └── utils.cpp
└── tests/
    ├── test_namespace.cpp
    ├── test_cgroup.cpp
    ├── test_seccomp.cpp
    ├── test_container.cpp
    └── test_http_server.cpp
```
