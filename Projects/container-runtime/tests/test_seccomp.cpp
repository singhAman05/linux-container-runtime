#include <gtest/gtest.h>
#include "seccomp_filter.h"

using namespace runtime;

TEST(SeccompTest, KernelSupportsSeccomp) {
    // Any modern Linux kernel (3.17+) should support seccomp.
    EXPECT_TRUE(seccomp_supported())
        << "seccomp(2) should be available on Linux >= 3.17";
}

TEST(SeccompTest, InstallFilterDoesNotThrow) {
    // install_seccomp_filter() should succeed on a supported kernel.
    // We can only call this once per process (filters stack and cannot be
    // removed), so we fork a child to test it in isolation.
    if (!seccomp_supported()) GTEST_SKIP() << "seccomp not supported";

    pid_t pid = fork();
    ASSERT_GE(pid, 0);

    if (pid == 0) {
        // Child: install filter and exit.
        install_seccomp_filter();
        _exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0)
        << "install_seccomp_filter() failed in child process";
}
