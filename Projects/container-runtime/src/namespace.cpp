#include "namespace.h"
#include "utils.h"

#include <sched.h>       // CLONE_NEW*
#include <unistd.h>      // chdir, chroot
#include <sys/mount.h>   // mount, umount2
#include <sys/stat.h>
#include <sys/syscall.h>
#include <cstring>
#include <stdexcept>

namespace runtime {

// Build the clone(2) flags for the requested namespace isolation.
// We always isolate: PID, MNT, UTS, IPC.
// NET isolation is optional (requires additional veth setup for real networking).
int namespace_flags(const ContainerSpec& spec) {
    int flags = CLONE_NEWPID   // new PID namespace: container sees itself as PID 1
              | CLONE_NEWNS    // new mount namespace: independent filesystem view
              | CLONE_NEWUTS   // new UTS namespace: own hostname/domainname
              | CLONE_NEWIPC;  // new IPC namespace: own semaphores/shared memory

    if (spec.enable_network)
        flags |= CLONE_NEWNET; // new network namespace: isolated network stack

    return flags;
}

// Called inside the child process after clone().
// Sets up the full namespace environment before exec().
void setup_namespaces(const ContainerSpec& spec) {
    // ── UTS: set container hostname ─────────────────────────────────────────
    if (!spec.hostname.empty()) {
        if (sethostname(spec.hostname.c_str(), spec.hostname.size()) != 0)
            throw_errno("sethostname");
    }

    // ── MNT: pivot into rootfs ───────────────────────────────────────────────
    // If a rootfs path is provided, chroot into it. In a production runtime
    // we would use pivot_root(2) for stronger isolation, but chroot is
    // sufficient to demonstrate the mount namespace boundary.
    if (!spec.rootfs.empty() && dir_exists(spec.rootfs)) {
        // Bind-mount rootfs onto itself so we can pivot_root later if desired.
        if (mount(spec.rootfs.c_str(), spec.rootfs.c_str(),
                  nullptr, MS_BIND | MS_REC, nullptr) != 0)
            throw_errno("mount --bind rootfs");

        if (chdir(spec.rootfs.c_str()) != 0)
            throw_errno("chdir rootfs");

        if (chroot(spec.rootfs.c_str()) != 0)
            throw_errno("chroot");

        if (chdir("/") != 0)
            throw_errno("chdir / after chroot");
    }

    // ── MNT: mount /proc in the new mount namespace ──────────────────────────
    mount_proc();
}

void mount_proc() {
    // Create /proc if it doesn't exist inside the new root.
    mkdir("/proc", 0755);

    if (mount("proc", "/proc", "proc",
              MS_NOSUID | MS_NOEXEC | MS_NODEV, nullptr) != 0) {
        // Non-fatal if /proc already mounted (e.g. no rootfs, using host FS).
        // In that case the existing /proc is in the new mount namespace already.
    }
}

void unmount_proc(const std::string& rootfs) {
    std::string proc_path = rootfs.empty() ? "/proc" : rootfs + "/proc";
    umount2(proc_path.c_str(), MNT_DETACH); // best-effort
}

} // namespace runtime
