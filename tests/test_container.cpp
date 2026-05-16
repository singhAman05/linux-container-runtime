#include <gtest/gtest.h>
#include "container.h"
#include "types.h"

using namespace runtime;

// ── ContainerManager lifecycle tests ─────────────────────────────────────────

class ContainerManagerTest : public ::testing::Test {
protected:
    ContainerManager mgr;
};

TEST_F(ContainerManagerTest, CreateReturnsNonEmptyId) {
    ContainerSpec spec;
    spec.command = {"/bin/true"};
    std::string id = mgr.create(spec);
    EXPECT_FALSE(id.empty());
}

TEST_F(ContainerManagerTest, CreateDefaultsHostname) {
    ContainerSpec spec;
    spec.command = {"/bin/true"};
    std::string id = mgr.create(spec);
    ContainerInfo info = mgr.get(id);
    EXPECT_FALSE(info.spec.hostname.empty())
        << "hostname should be auto-generated";
}

TEST_F(ContainerManagerTest, CreateStateIsCreated) {
    ContainerSpec spec;
    spec.command = {"/bin/true"};
    std::string id = mgr.create(spec);
    EXPECT_EQ(mgr.get(id).state, ContainerState::Created);
}

TEST_F(ContainerManagerTest, DuplicateIdThrows) {
    ContainerSpec spec;
    spec.id      = "fixed-id";
    spec.command = {"/bin/true"};
    mgr.create(spec);
    EXPECT_THROW(mgr.create(spec), std::runtime_error);
}

TEST_F(ContainerManagerTest, GetNonexistentThrows) {
    EXPECT_THROW(mgr.get("no-such-id"), std::out_of_range);
}

TEST_F(ContainerManagerTest, ListReflectsCreatedContainers) {
    // Fresh manager local to this test to avoid cross-test state.
    ContainerManager local_mgr;
    for (int i = 0; i < 3; ++i) {
        ContainerSpec spec;
        spec.command = {"/bin/true"};
        local_mgr.create(spec);
    }
    EXPECT_EQ(local_mgr.list().size(), 3u);
}

TEST_F(ContainerManagerTest, DestroyCreatedContainer) {
    ContainerSpec spec;
    spec.command = {"/bin/true"};
    std::string id = mgr.create(spec);
    EXPECT_NO_THROW(mgr.destroy(id));
    EXPECT_THROW(mgr.get(id), std::out_of_range);
}

TEST_F(ContainerManagerTest, DestroyNonexistentThrows) {
    EXPECT_THROW(mgr.destroy("ghost"), std::out_of_range);
}

TEST_F(ContainerManagerTest, StopNonrunningThrows) {
    ContainerSpec spec;
    spec.command = {"/bin/true"};
    std::string id = mgr.create(spec);
    EXPECT_THROW(mgr.stop(id), std::runtime_error);
}

TEST(StateToStringTest, AllStates) {
    EXPECT_EQ(state_to_string(ContainerState::Created),   "created");
    EXPECT_EQ(state_to_string(ContainerState::Running),   "running");
    EXPECT_EQ(state_to_string(ContainerState::Stopped),   "stopped");
    EXPECT_EQ(state_to_string(ContainerState::Destroyed), "destroyed");
}
