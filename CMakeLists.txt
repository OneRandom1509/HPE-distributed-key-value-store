cmake_minimum_required(VERSION 3.16)

project(KVServerProject VERSION 1.0 LANGUAGES CXX)

# ------------------------------------------------
# C++ settings
# ------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()

# ------------------------------------------------
# Dependencies
# ------------------------------------------------

find_package(PkgConfig REQUIRED)

pkg_check_modules(MARGO REQUIRED IMPORTED_TARGET margo)
pkg_check_modules(ABT REQUIRED IMPORTED_TARGET argobots)
pkg_check_modules(MERCURY REQUIRED IMPORTED_TARGET mercury)

find_package(thallium REQUIRED)
find_package(Boost REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(GTest REQUIRED)
find_package(spdlog REQUIRED)

# ------------------------------------------------
# Server
# ------------------------------------------------

add_executable(kvm_server
    src/KVServer.cpp
    src/KVStore.cpp
    src/Timer.cpp
    src/main_server.cpp
    src/MurmurHash3.cpp
    src/GossipMembership.cpp
    src/ConsistentHashRing.cpp
    src/config.cpp
)

target_include_directories(kvm_server
    PRIVATE
        include
)

target_link_libraries(kvm_server
    PRIVATE
        PkgConfig::MARGO
        PkgConfig::ABT
        PkgConfig::MERCURY
        thallium
        Boost::boost
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# ------------------------------------------------
# Client
# ------------------------------------------------

add_executable(kvm_client
    src/KVClient.cpp
    src/KVStore.cpp
    src/KVDistributor.cpp
    src/ConsistentHashRing.cpp
    src/config.cpp
    src/Timer.cpp
    src/main_client.cpp
    src/MurmurHash3.cpp
)

target_include_directories(kvm_client
    PRIVATE
        include
)

target_link_libraries(kvm_client
    PRIVATE
        PkgConfig::MARGO
        PkgConfig::ABT
        PkgConfig::MERCURY
        thallium
        Boost::boost
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# ------------------------------------------------
# Scripts
# ------------------------------------------------

configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/start_nodes.sh
    ${CMAKE_BINARY_DIR}/start_nodes.sh
    COPYONLY
)

add_custom_command(
    TARGET kvm_server
    POST_BUILD
    COMMAND chmod +x ${CMAKE_BINARY_DIR}/start_nodes.sh
    COMMENT "Making start_nodes.sh executable"
)

# ------------------------------------------------
# Tests
# ------------------------------------------------

add_executable(consistent_hash_ring_tests
    src/ConsistentHashRing.cpp
    src/MurmurHash3.cpp
    tests/ConsistentHashRingTests.cpp
)

target_include_directories(consistent_hash_ring_tests
    PRIVATE
        include
)

add_test(NAME consistent_hash_ring_tests COMMAND consistent_hash_ring_tests)

add_executable(kv_distributor_tests
    src/KVDistributor.cpp
    src/KVClient.cpp
    src/KVStore.cpp
    src/ConsistentHashRing.cpp
    src/config.cpp
    src/MurmurHash3.cpp
    src/Timer.cpp
    tests/KVDistributorTests.cpp
)
target_include_directories(kv_distributor_tests
    PRIVATE
        include
)
target_link_libraries(kv_distributor_tests
    PRIVATE
        PkgConfig::MARGO
        PkgConfig::ABT
        PkgConfig::MERCURY
        thallium
        Boost::boost
        nlohmann_json::nlohmann_json
        GTest::gtest_main
    spdlog::spdlog
)
target_compile_definitions(kv_distributor_tests PRIVATE -DTESTING)
add_test(NAME kv_distributor_tests COMMAND kv_distributor_tests)


# Might be good for later

# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     target_compile_options(kvm_server PRIVATE -fsanitize=address -g)
#     target_link_options(kvm_server PRIVATE -fsanitize=address)
#
#     target_compile_options(kvm_client PRIVATE -fsanitize=address -g)
#     target_link_options(kvm_client PRIVATE -fsanitize=address)
# endif()
