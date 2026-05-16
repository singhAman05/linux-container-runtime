#pragma once

namespace runtime {

// Install a seccomp-bpf allowlist filter on the calling thread.
//
// The filter permits a minimal set of syscalls required for a basic
// process to run (read, write, exit, mmap, …) while killing any process
// that attempts a forbidden syscall (e.g. ptrace, mount, pivot_root from
// inside the container, kexec_load, …).
//
// Must be called after all namespace / cgroup setup is done, just before
// execve() of the container workload.
void install_seccomp_filter();

// Returns true if the running kernel supports seccomp(2).
bool seccomp_supported();

} // namespace runtime
