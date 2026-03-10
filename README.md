# Distributed Key-Value Store

A distributed, scalable and persistent Key-Value store using a shared memory map and enabling access across all nodes in a cluster using an RPC/RDMA framework.

## Project Overview

This project aims to design and implement a distributed key-value store with the following capabilities:

- Configure nodes based on requirements and preference.
- Store a large number of key-value pairs using a shared memory map.
- Distribute key-value pairs across nodes using `key % number_of_nodes` logic.
- Keys are of type integer, and values are of type string.
- Automatically redistribute and load-balance key-value pairs when nodes are added or removed.
- Support both local and remote fetches along with other key-value operations.

## Features

- PUT key-value pair  
- GET key-value pair  
- UPDATE key-value pair  
- DELETE key-value pair   

## Dependencies

Ensure the following dependencies are installed:

- CMake (version 3.10 or higher)
- C++ Compiler with C++14 support (e.g., GCC or Clang)
- Spack (for managing HPC dependencies)
- Boost
- nlohmann-json (via vcpkg)
- Mochi libraries:
  - Margo
  - Argobots
  - Mercury
  - Thallium


## Install Dependencies with Spack

### Clone Spack
```
git clone https://github.com/spack/spack.git
export PATH=$PATH:$(pwd)/spack/bin
```

### Install required libraries
```
spack install margo
spack install argobots
spack install mercury
spack install thallium
spack install boost
```

### Load the Spack environment
```
spack load margo argobots mercury thallium boost
```

### Installing nlohmann-json (via vcpkg)
```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

On windows:
.\bootstrap-vcpkg.bat

On linux/macOS:
./bootstrap-vcpkg.sh

./vcpkg install nlohmann-json


```

## Clone the repository and switch to the integration branch

### Create and navigate to the build directory
```
mkdir build
cd build
```

### Configure with CMake
```
cmake ..
```

### Build the project
```
make
```

## Starting the server
### Default memory allocation
```
./start_nodes.sh
```
The above commands can be run on multiple machines to start the server across the cluster.

## Starting the client
### Default (will prompt to add nodes)
```
./kvm_client
```

## Stopping the server
```
CTRL+C
```

## Stopping the client
```
Type exit or click CTRL+C
```

