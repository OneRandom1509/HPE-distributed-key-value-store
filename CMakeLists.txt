cmake_minimum_required(VERSION 3.10)
project(KVServerProject VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_FLAGS "-lrt")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# Find required packages using Spack paths
find_package(PkgConfig REQUIRED)
# Include paths and libraries for the Mochi libraries
pkg_check_modules(MARGO REQUIRED IMPORTED_TARGET margo)
pkg_check_modules(ABT REQUIRED IMPORTED_TARGET argobots)
pkg_check_modules(MERCURY REQUIRED IMPORTED_TARGET mercury)
# Find Thallium
find_package(thallium REQUIRED)
# Find Boost (header-only libraries or specific ones if needed)
find_package(Boost REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
# Include directories
include_directories(include)
include_directories(${Boost_INCLUDE_DIRS})
# Add server executable
add_executable(kvm_server src/KVServer.cpp src/KVStore.cpp src/main_server.cpp)
# Link the libraries (Margo, Argobots, Mercury, Thallium, Boost)
target_link_libraries(kvm_server
    PkgConfig::MARGO
    PkgConfig::ABT
    PkgConfig::MERCURY
    thallium
    Boost::boost
    stdc++fs
)
target_include_directories(kvm_server PRIVATE
    ${MARGO_INCLUDE_DIRS}
    ${ABT_INCLUDE_DIRS}
    ${MERCURY_INCLUDE_DIRS}
    ${THALLIUM_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
# Add client executable
add_executable(kvm_client src/KVClient.cpp src/KVStore.cpp src/KVDistributor.cpp src/config.cpp src/main_client.cpp)
target_link_libraries(kvm_client
    PkgConfig::MARGO
    PkgConfig::ABT
    PkgConfig::MERCURY
    thallium
    Boost::boost
    stdc++fs
    nlohmann_json::nlohmann_json
)
target_include_directories(kvm_client PRIVATE
    ${MARGO_INCLUDE_DIRS}
    ${ABT_INCLUDE_DIRS}
    ${MERCURY_INCLUDE_DIRS}
    ${THALLIUM_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/start_nodes.sh
    ${CMAKE_BINARY_DIR}/start_nodes.sh
    COPYONLY
)
# Make the script executable after build
add_custom_command(
    TARGET kvm_server
    POST_BUILD
    COMMAND chmod +x ${CMAKE_BINARY_DIR}/start_nodes.sh
    COMMENT "Making start_nodes.sh executable"
)
