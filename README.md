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
- Spack libraries:
  - mochi-margo
  - argobots
  - mercury
  - mochi-thallium
  - nlohmann-json
  - boost

## Install Dependencies with Spack

### Clone Spack
```bash
git clone https://github.com/spack/spack.git
export PATH=$PATH:$(pwd)/spack/bin
```

> [!TIP]
> It's better if you add the `/bin` directory of the spack repo to your path, so you don't have to do the above commands for every new shell session.


### Install required libraries
```bash
git clone https://github.com/mochi-hpc/mochi-spack-packages.git
spack repo add mochi-spack-packages

spack install margo
spack install argobots
spack install mercury
spack install thallium
spack install nlohmann-json
spack install boost
```

### Load the Spack environment
```
spack load margo argobots mercury thallium boost nlohmann-json
```
Now clone the repository and switch to the desired branch

## Build the project
```bash
make clean # removes the build directories for a fresh build
make [build|debug]
```

## Starting the server
### With default memory allocation
```bash
make server
```

### With custom options 
```bash
make server ARGS="[protocol] [port] [shared_mem_size][K|M|G] [persistent/memory]" # all are optional in order
```
The above command can be run on multiple machines to start the server across the cluster.

To stop the client, just do `CTRL+C`.

## Starting the client
### Default (will prompt to add nodes)
```bash
make client
```
To stop the client, just type `exit` or do `CTRL+C`.
