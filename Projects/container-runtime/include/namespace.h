#pragma once

#include "types.h"
#include <string>

namespace runtime {

// Flags passed to clone(2) to create a new container process in isolated namespaces.
// We use: PID, MNT, UTS, IPC, and optionally NET.
int namespace_flags(const ContainerSpec& spec);

// Called inside the child process (after clone) to finish namespace setup:
//   - set hostname in UTS namespace
//   - pivot_root / chroot into rootfs
//   - mount /proc inside the new mount namespace
void setup_namespaces(const ContainerSpec& spec);

// Mount /proc in the new mount namespace so tools like ps work correctly.
void mount_proc();

// Unmount /proc and clean up mounts on container exit.
void unmount_proc(const std::string& rootfs);

} // namespace runtime
