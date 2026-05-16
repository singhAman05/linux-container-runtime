#include "seccomp_filter.h"
#include "utils.h"

#include <seccomp.h>
#include <stdexcept>
#include <sys/utsname.h>
#include <unistd.h>

namespace runtime {

bool seccomp_supported() {
    // seccomp(2) was added in Linux 3.17.
    // A simple check: try to read /proc/version and look for kernel version.
    struct utsname u{};
    if (uname(&u) != 0) return false;
    // Parse major.minor from u.release (e.g. "6.1.0-28-amd64")
    int major = 0, minor = 0;
    sscanf(u.release, "%d.%d", &major, &minor);
    return (major > 3) || (major == 3 && minor >= 17);
}

// Install a seccomp-bpf ALLOWLIST (default-deny) filter.
//
// Only the syscalls explicitly listed below are permitted.
// Any other syscall triggers SCMP_ACT_KILL_PROCESS — the kernel kills
// the offending process immediately without delivering a signal.
//
// This mirrors the approach used by Firecracker's jailer and OCI runtimes
// like runc / containerd.
void install_seccomp_filter() {
    if (!seccomp_supported()) {
        log_warn("seccomp not supported on this kernel — skipping filter");
        return;
    }

    // SCMP_ACT_KILL_PROCESS: kill entire process (not just thread) on violation.
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (!ctx) throw std::runtime_error("seccomp_init failed");

    // Helper lambda — add a syscall to the allowlist.
    auto allow = [&](int syscall_nr) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscall_nr, 0) != 0)
            throw std::runtime_error("seccomp_rule_add failed for syscall " +
                                     std::to_string(syscall_nr));
    };

    // ── Process / thread lifecycle ───────────────────────────────────────────
    allow(SCMP_SYS(exit));
    allow(SCMP_SYS(exit_group));
    allow(SCMP_SYS(fork));
    allow(SCMP_SYS(vfork));
    allow(SCMP_SYS(clone));
    allow(SCMP_SYS(clone3));
    allow(SCMP_SYS(execve));
    allow(SCMP_SYS(execveat));
    allow(SCMP_SYS(wait4));
    allow(SCMP_SYS(waitid));
    allow(SCMP_SYS(getpid));
    allow(SCMP_SYS(getppid));
    allow(SCMP_SYS(gettid));
    allow(SCMP_SYS(getpgid));
    allow(SCMP_SYS(setpgid));
    allow(SCMP_SYS(setsid));
    allow(SCMP_SYS(kill));
    allow(SCMP_SYS(tgkill));
    allow(SCMP_SYS(tkill));
    allow(SCMP_SYS(rt_sigaction));
    allow(SCMP_SYS(rt_sigprocmask));
    allow(SCMP_SYS(rt_sigreturn));
    allow(SCMP_SYS(sigaltstack));

    // ── Memory management ────────────────────────────────────────────────────
    allow(SCMP_SYS(brk));
    allow(SCMP_SYS(mmap));
    allow(SCMP_SYS(munmap));
    allow(SCMP_SYS(mprotect));
    allow(SCMP_SYS(mremap));
    allow(SCMP_SYS(madvise));
    allow(SCMP_SYS(mincore));

    // ── File I/O ─────────────────────────────────────────────────────────────
    allow(SCMP_SYS(read));
    allow(SCMP_SYS(write));
    allow(SCMP_SYS(readv));
    allow(SCMP_SYS(writev));
    allow(SCMP_SYS(pread64));
    allow(SCMP_SYS(pwrite64));
    allow(SCMP_SYS(open));
    allow(SCMP_SYS(openat));
    allow(SCMP_SYS(openat2));
    allow(SCMP_SYS(close));
    allow(SCMP_SYS(close_range));
    allow(SCMP_SYS(read));
    allow(SCMP_SYS(lseek));
    allow(SCMP_SYS(stat));
    allow(SCMP_SYS(fstat));
    allow(SCMP_SYS(lstat));
    allow(SCMP_SYS(newfstatat));
    allow(SCMP_SYS(statx));
    allow(SCMP_SYS(access));
    allow(SCMP_SYS(faccessat));
    allow(SCMP_SYS(dup));
    allow(SCMP_SYS(dup2));
    allow(SCMP_SYS(dup3));
    allow(SCMP_SYS(pipe));
    allow(SCMP_SYS(pipe2));
    allow(SCMP_SYS(fcntl));
    allow(SCMP_SYS(ioctl));
    allow(SCMP_SYS(getdents));
    allow(SCMP_SYS(getdents64));
    allow(SCMP_SYS(getcwd));
    allow(SCMP_SYS(chdir));
    allow(SCMP_SYS(mkdir));
    allow(SCMP_SYS(mkdirat));
    allow(SCMP_SYS(rmdir));
    allow(SCMP_SYS(unlink));
    allow(SCMP_SYS(unlinkat));
    allow(SCMP_SYS(rename));
    allow(SCMP_SYS(renameat));
    allow(SCMP_SYS(renameat2));
    allow(SCMP_SYS(chmod));
    allow(SCMP_SYS(fchmod));
    allow(SCMP_SYS(chown));
    allow(SCMP_SYS(fchown));
    allow(SCMP_SYS(lchown));
    allow(SCMP_SYS(truncate));
    allow(SCMP_SYS(ftruncate));
    allow(SCMP_SYS(fsync));
    allow(SCMP_SYS(fdatasync));
    allow(SCMP_SYS(sync));
    allow(SCMP_SYS(sendfile));
    allow(SCMP_SYS(copy_file_range));

    // ── Networking (basic) ───────────────────────────────────────────────────
    allow(SCMP_SYS(socket));
    allow(SCMP_SYS(connect));
    allow(SCMP_SYS(accept));
    allow(SCMP_SYS(accept4));
    allow(SCMP_SYS(bind));
    allow(SCMP_SYS(listen));
    allow(SCMP_SYS(getsockname));
    allow(SCMP_SYS(getpeername));
    allow(SCMP_SYS(getsockopt));
    allow(SCMP_SYS(setsockopt));
    allow(SCMP_SYS(sendto));
    allow(SCMP_SYS(recvfrom));
    allow(SCMP_SYS(sendmsg));
    allow(SCMP_SYS(recvmsg));
    allow(SCMP_SYS(shutdown));

    // ── Polling / event I/O ──────────────────────────────────────────────────
    allow(SCMP_SYS(poll));
    allow(SCMP_SYS(ppoll));
    allow(SCMP_SYS(select));
    allow(SCMP_SYS(pselect6));
    allow(SCMP_SYS(epoll_create));
    allow(SCMP_SYS(epoll_create1));
    allow(SCMP_SYS(epoll_ctl));
    allow(SCMP_SYS(epoll_wait));
    allow(SCMP_SYS(epoll_pwait));
    allow(SCMP_SYS(eventfd));
    allow(SCMP_SYS(eventfd2));
    allow(SCMP_SYS(timerfd_create));
    allow(SCMP_SYS(timerfd_settime));
    allow(SCMP_SYS(timerfd_gettime));

    // ── Time ─────────────────────────────────────────────────────────────────
    allow(SCMP_SYS(clock_gettime));
    allow(SCMP_SYS(clock_nanosleep));
    allow(SCMP_SYS(nanosleep));
    allow(SCMP_SYS(gettimeofday));
    allow(SCMP_SYS(time));
    allow(SCMP_SYS(times));

    // ── IDs / credentials ────────────────────────────────────────────────────
    allow(SCMP_SYS(getuid));
    allow(SCMP_SYS(geteuid));
    allow(SCMP_SYS(getgid));
    allow(SCMP_SYS(getegid));
    allow(SCMP_SYS(getgroups));
    allow(SCMP_SYS(setuid));
    allow(SCMP_SYS(setgid));
    allow(SCMP_SYS(setresuid));
    allow(SCMP_SYS(setresgid));

    // ── Misc ─────────────────────────────────────────────────────────────────
    allow(SCMP_SYS(uname));
    allow(SCMP_SYS(arch_prctl));
    allow(SCMP_SYS(prctl));
    allow(SCMP_SYS(futex));
    allow(SCMP_SYS(set_tid_address));
    allow(SCMP_SYS(set_robust_list));
    allow(SCMP_SYS(rseq));
    allow(SCMP_SYS(getrandom));
    allow(SCMP_SYS(sched_yield));
    allow(SCMP_SYS(sched_getaffinity));
    allow(SCMP_SYS(sched_setaffinity));
    allow(SCMP_SYS(getrlimit));
    allow(SCMP_SYS(setrlimit));
    allow(SCMP_SYS(prlimit64));
    allow(SCMP_SYS(mlock));
    allow(SCMP_SYS(munlock));
    allow(SCMP_SYS(symlink));
    allow(SCMP_SYS(symlinkat));
    allow(SCMP_SYS(readlink));
    allow(SCMP_SYS(readlinkat));
    allow(SCMP_SYS(umask));
    allow(SCMP_SYS(statfs));
    allow(SCMP_SYS(fstatfs));

    // NOTE: The following are intentionally DENIED (not in allowlist):
    //   ptrace        – no container should trace host or sibling processes
    //   mount         – containers cannot remount filesystems
    //   pivot_root    – only the runtime itself uses this
    //   kexec_load    – cannot load a new kernel
    //   init_module   – cannot load kernel modules
    //   perf_event_open – side-channel risk
    //   bpf           – containers cannot install BPF programs
    //   io_uring_*    – reduce attack surface

    if (seccomp_load(ctx) != 0) {
        seccomp_release(ctx);
        throw std::runtime_error("seccomp_load failed");
    }

    seccomp_release(ctx);
    log_info("seccomp-bpf allowlist filter installed");
}

} // namespace runtime
