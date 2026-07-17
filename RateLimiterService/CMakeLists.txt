cmake_minimum_required(VERSION 3.15)
project(RateLimiterService CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Visual Studio: group generated targets/filters nicely and avoid min/max macro clashes.
if (MSVC)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# core_lib: algorithm-agnostic engine (strategies, storage, config, logging).
# Shared by both the HTTP service and the test executable so the tests can
# exercise the exact same logic without needing an HTTP layer.
# ---------------------------------------------------------------------------
add_library(core_lib STATIC
    src/Logger.cpp
    src/ConfigManager.cpp
    src/RateLimiterCore.cpp
    src/FixedWindowStrategy.cpp
    src/SlidingWindowStrategy.cpp
    src/TokenBucketStrategy.cpp
)
target_include_directories(core_lib PUBLIC include)
target_link_libraries(core_lib PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# RateLimiterService: the HTTP service (main executable).
# ---------------------------------------------------------------------------
add_executable(RateLimiterService
    src/main.cpp
    src/ApiServer.cpp
)
target_include_directories(RateLimiterService PRIVATE include third_party)
target_link_libraries(RateLimiterService PRIVATE core_lib)

# ---------------------------------------------------------------------------
# RateLimiterTests: standalone checks against core_lib (no HTTP dependency).
# ---------------------------------------------------------------------------
add_executable(RateLimiterTests
    tests/test_main.cpp
)
target_include_directories(RateLimiterTests PRIVATE include)
target_link_libraries(RateLimiterTests PRIVATE core_lib)

enable_testing()
add_test(NAME RateLimiterTests COMMAND RateLimiterTests)
