cmake_minimum_required(VERSION 3.16)
project(container-runtime VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic")

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(SECCOMP REQUIRED libseccomp)
find_package(Threads REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

# ── include dirs ──────────────────────────────────────────────
include_directories(include)

# ── core library (shared between main binary and tests) ───────
add_library(runtime_core STATIC
    src/namespace.cpp
    src/cgroup.cpp
    src/seccomp_filter.cpp
    src/container.cpp
    src/http_server.cpp
    src/utils.cpp
)

target_include_directories(runtime_core PUBLIC
    include
    ${SECCOMP_INCLUDE_DIRS}
)

target_link_libraries(runtime_core PUBLIC
    ${SECCOMP_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# ── main binary ───────────────────────────────────────────────
add_executable(container-runtime src/main.cpp)
target_link_libraries(container-runtime PRIVATE runtime_core)

# ── tests ─────────────────────────────────────────────────────
enable_testing()

find_package(GTest REQUIRED)

add_executable(runtime_tests
    tests/test_cgroup.cpp
    tests/test_namespace.cpp
    tests/test_seccomp.cpp
    tests/test_container.cpp
    tests/test_http_server.cpp
)

target_link_libraries(runtime_tests PRIVATE
    runtime_core
    GTest::GTest
    GTest::Main
)

include(GoogleTest)
gtest_discover_tests(runtime_tests)

# ── install ───────────────────────────────────────────────────
install(TARGETS container-runtime DESTINATION bin)
