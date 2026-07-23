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
  - mochi-thallium
  - nlohmann-json
  - boost
  - spdlog

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
cd mochi-spack-packages/spack_repo
spack repo add mochi

spack install mochi-margo
spack install mochi-thallium
spack install nlohmann-json
spack install boost
spack install spdlog
```

### Load the Spack environment
```
spack load mochi-margo mochi-thallium nlohmann-json boost spdlog
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

To stop the server, just do `CTRL+C`.

## Starting the client
### Default (will prompt to add nodes)
```bash
make client
```
To stop the client, just type `exit` or do `CTRL+C`.

## Configuration

The server and client read `config/config.json` (JSON format). Below are the available fields:

| Field | Type | Description |
|---|---|---|
| `provider_id` | int | RPC provider ID (default: `1`) |
| `protocol` | string | OFI protocol (e.g. `ofi+tcp`) |
| `count_of_node` | int | Total number of nodes in the cluster |
| `ip_addresses` | object | Node ID to `ip:port` mapping |
| `size` | int | Shared memory size in MB |
| `local_ip` | string | Override for this node's `ip:port` (empty string to auto-detect) |
| `gossip` | object (optional) | Gossip protocol settings (see below) |

### Gossip sub-fields

| Field | Type | Default | Description |
|---|---|---|---|
| `interval_ms` | int | `1000` | Gossip interval in ms |
| `suspect_threshold_ms` | int | `3000` | Time before marking a node as suspect |
| `dead_threshold_ms` | int | `10000` | Time before marking a node as dead |
| `seed_nodes` | string[] | `[]` | Initial gossip seed endpoints (`protocol://ip:port`)

An example configuration file: [sample_config.json](https://github.com/OneRandom1509/hpe-distributed-key-value-store/blob/feat/gossip-membership/config/sample_config.json)

## Benchmark Results

Benchmarks conducted on a **4-node KV server cluster** with **10,000 operations** in **MEMORY storage mode**.

| Metric | Sequential | Random |
|---|---|---|
| Total Insertion Time | 2630 ms | 3492 ms |
| Avg Insertion Time | 0.2630 ms | 0.3492 ms |
| Total Fetch Time | 2133 ms | 1937 ms |
| Avg Fetch Time | 0.2128 ms | 0.1932 ms |
| Min Fetch Time | 0.0240 ms | 0.0250 ms |
| Max Fetch Time | 329.1320 ms | 95.8230 ms |
| Successful Fetches | 10,000/10,000 | 10,000/10,000 |
