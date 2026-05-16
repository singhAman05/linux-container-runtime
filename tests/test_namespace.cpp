#include <gtest/gtest.h>
#include "namespace.h"
#include "types.h"

using namespace runtime;

TEST(NamespaceFlagsTest, AlwaysIncludesPIDMntUtsIpc) {
    ContainerSpec spec;
    spec.enable_network = false;
    int flags = namespace_flags(spec);

    EXPECT_NE(flags & CLONE_NEWPID, 0) << "PID namespace must be set";
    EXPECT_NE(flags & CLONE_NEWNS,  0) << "Mount namespace must be set";
    EXPECT_NE(flags & CLONE_NEWUTS, 0) << "UTS namespace must be set";
    EXPECT_NE(flags & CLONE_NEWIPC, 0) << "IPC namespace must be set";
}

TEST(NamespaceFlagsTest, NetworkNamespaceOptional) {
    ContainerSpec spec;
    spec.enable_network = false;
    int flags_no_net = namespace_flags(spec);
    EXPECT_EQ(flags_no_net & CLONE_NEWNET, 0);

    spec.enable_network = true;
    int flags_with_net = namespace_flags(spec);
    EXPECT_NE(flags_with_net & CLONE_NEWNET, 0);
}
