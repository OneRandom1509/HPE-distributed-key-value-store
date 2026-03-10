#include "KVClient.hpp"
#include "KVStore.hpp"
#include "KVDistributor.hpp"
#include "config.hpp"
#include <limits>
#include <chrono>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>
#include <thread>
#include <numeric>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include "config.hpp"

// Helper function to parse command arguments
std::vector<std::string> parseCommand(const std::string& command) {
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    return args;
}

// Help function
void printHelp() {
    std::cout << "\nDistributed Key-Value Store Commands:" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "  put <key> <value>        - Store a key-value pair" << std::endl;
    std::cout << "  get <key>                - Get a value for a key" << std::endl;
    std::cout << "  update <key> <value>     - Update an existing key-value pair" << std::endl;
    std::cout << "  delete <key>             - Delete a key-value pair" << std::endl;
    std::cout << "  benchmark                - To run with sequential fetch pattern" << std::endl;
    std::cout << "  benchmark1               - Run benchmark with random fetch pattern" << std::endl;
    std::cout << "  help                     - Show this help message" << std::endl;
    std::cout << "  exit                     - Exit the program" << std::endl;
}


// Add these function declarations after the printHelp() function and before main()

// Function to generate a random string of specified length
std::string generateRandomString(int length) {
    const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.size() - 1);
    
    std::string result;
    result.reserve(length);
    for (int i = 0; i < length; ++i) {
        result += charset[dis(gen)];
    }
    return result;
}

// Helper function to determine if a key is local based on consistent hashing
bool isKeyLocal(int key, int local_node_id, int total_nodes) {
    // Simple modulo-based distribution (modify this based on your actual hash function)
    int target_node = key % total_nodes;
    return target_node == local_node_id;
}

// Benchmark function with sequential fetch pattern
void benchmark(KVDistributor& distributor) {
    const int NUM_OPERATIONS = 10000;
    const int VALUE_SIZE = 3;
    const int LOCAL_NODE_ID = 0; // From your config file
    const int TOTAL_NODES = 2;   // From your config file
    
    std::cout << "\n=== BENCHMARK (Sequential Fetch Pattern) ===" << std::endl;
    std::cout << "Inserting " << NUM_OPERATIONS << " key-value pairs..." << std::endl;
    
    // Phase 1: Sequential Insertion
    auto start_insert = std::chrono::high_resolution_clock::now();
    for (int i = 1; i <= NUM_OPERATIONS; ++i) {
        try {
            std::string value = generateRandomString(VALUE_SIZE);
            distributor.insert(i, value);
        } catch (const std::exception& e) {
            std::cout << "Insert error for key " << i << ": " << e.what() << std::endl;
        }
    }
    auto end_insert = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_insert - start_insert);
    std::cout << "Insertion completed!" << std::endl;
    
    // Phase 2: Sequential Fetch with local/remote tracking
    std::cout << "\nFetching " << NUM_OPERATIONS << " key-value pairs sequentially..." << std::endl;
    std::vector<double> fetch_times;
    std::vector<double> local_fetch_times;
    std::vector<double> remote_fetch_times;
    fetch_times.reserve(NUM_OPERATIONS);
    local_fetch_times.reserve(NUM_OPERATIONS);
    remote_fetch_times.reserve(NUM_OPERATIONS);
    
    auto start_fetch_all = std::chrono::high_resolution_clock::now();
    for (int i = 1; i <= NUM_OPERATIONS; ++i) {
        try {
            // Check if key exists locally based on hash distribution
            bool is_local = isKeyLocal(i, LOCAL_NODE_ID, TOTAL_NODES);
            
            auto start_single_fetch = std::chrono::high_resolution_clock::now();
            std::string value = distributor.get(i);
            auto end_single_fetch = std::chrono::high_resolution_clock::now();
            
            auto single_fetch_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_single_fetch - start_single_fetch);
            double fetch_time_ms = static_cast<double>(single_fetch_duration.count()) / 1000.0;
            
            fetch_times.push_back(fetch_time_ms);
            
            if (is_local) {
                local_fetch_times.push_back(fetch_time_ms);
            } else {
                remote_fetch_times.push_back(fetch_time_ms);
            }
            
        } catch (const std::exception& e) {
            std::cout << "Fetch error for key " << i << ": " << e.what() << std::endl;
        }
    }
    auto end_fetch_all = std::chrono::high_resolution_clock::now();
    auto total_fetch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_fetch_all - start_fetch_all);
    
    // Calculate overall statistics
    if (!fetch_times.empty()) {
        double total_time = std::accumulate(fetch_times.begin(), fetch_times.end(), 0.0);
        double avg_time = total_time / fetch_times.size();
        double min_time = *std::min_element(fetch_times.begin(), fetch_times.end());
        double max_time = *std::max_element(fetch_times.begin(), fetch_times.end());
        
        std::cout << "\n=== SEQUENTIAL BENCHMARK RESULTS ===" << std::endl;
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "--- INSERTION METRICS ---" << std::endl;
        std::cout << "Total insertion time: " << insert_duration.count() << " ms" << std::endl;
        std::cout << "Average insertion time per operation: " <<
                     static_cast<double>(insert_duration.count()) / NUM_OPERATIONS << " ms" << std::endl;
        
        std::cout << "--- OVERALL FETCH METRICS ---" << std::endl;
        std::cout << "Total fetch time: " << total_fetch_duration.count() << " ms" << std::endl;
        std::cout << "Average fetch time: " << avg_time << " ms" << std::endl;
        std::cout << "Minimum fetch time: " << min_time << " ms" << std::endl;
        std::cout << "Maximum fetch time: " << max_time << " ms" << std::endl;
        std::cout << "Successful fetches: " << fetch_times.size() << "/" << NUM_OPERATIONS << std::endl;
        
        // Local fetch statistics
        if (!local_fetch_times.empty()) {
            double local_total = std::accumulate(local_fetch_times.begin(), local_fetch_times.end(), 0.0);
            double local_avg = local_total / local_fetch_times.size();
            double local_min = *std::min_element(local_fetch_times.begin(), local_fetch_times.end());
            double local_max = *std::max_element(local_fetch_times.begin(), local_fetch_times.end());
            
            std::cout << "--- LOCAL FETCH METRICS ---" << std::endl;
            std::cout << "Local fetches count: " << local_fetch_times.size() << std::endl;
            std::cout << "Local fetch percentage: " << 
                         (static_cast<double>(local_fetch_times.size()) / fetch_times.size()) * 100.0 << "%" << std::endl;
            std::cout << "Average local fetch time: " << local_avg << " ms" << std::endl;
            std::cout << "Minimum local fetch time: " << local_min << " ms" << std::endl;
            std::cout << "Maximum local fetch time: " << local_max << " ms" << std::endl;
            std::cout << "Total local fetch time: " << local_total << " ms" << std::endl;
        } else {
            std::cout << "--- LOCAL FETCH METRICS ---" << std::endl;
            std::cout << "No local fetches performed" << std::endl;
        }
        
        // Remote fetch statistics
        if (!remote_fetch_times.empty()) {
            double remote_total = std::accumulate(remote_fetch_times.begin(), remote_fetch_times.end(), 0.0);
            double remote_avg = remote_total / remote_fetch_times.size();
            double remote_min = *std::min_element(remote_fetch_times.begin(), remote_fetch_times.end());
            double remote_max = *std::max_element(remote_fetch_times.begin(), remote_fetch_times.end());
            
            std::cout << "--- REMOTE FETCH METRICS ---" << std::endl;
            std::cout << "Remote fetches count: " << remote_fetch_times.size() << std::endl;
            std::cout << "Remote fetch percentage: " << 
                         (static_cast<double>(remote_fetch_times.size()) / fetch_times.size()) * 100.0 << "%" << std::endl;
            std::cout << "Average remote fetch time: " << remote_avg << " ms" << std::endl;
            std::cout << "Minimum remote fetch time: " << remote_min << " ms" << std::endl;
            std::cout << "Maximum remote fetch time: " << remote_max << " ms" << std::endl;
            std::cout << "Total remote fetch time: " << remote_total << " ms" << std::endl;
        } else {
            std::cout << "--- REMOTE FETCH METRICS ---" << std::endl;
            std::cout << "No remote fetches performed" << std::endl;
        }
    }
}

// Benchmark function with random fetch pattern
void benchmark1(KVDistributor& distributor) {
    const int NUM_OPERATIONS = 10000;
    const int VALUE_SIZE = 3;
    const int LOCAL_NODE_ID = 0; // From your config file
    const int TOTAL_NODES = 2;   // From your config file
    
    std::cout << "\n=== BENCHMARK1 (Random Fetch Pattern) ===" << std::endl;
    std::cout << "Inserting " << NUM_OPERATIONS << " key-value pairs..." << std::endl;
    
    // Phase 1: Sequential Insertion (same as benchmark)
    auto start_insert = std::chrono::high_resolution_clock::now();
    for (int i = 1; i <= NUM_OPERATIONS; ++i) {
        try {
            std::string value = generateRandomString(VALUE_SIZE);
            distributor.insert(i, value);
        } catch (const std::exception& e) {
            std::cout << "Insert error for key " << i << ": " << e.what() << std::endl;
        }
    }
    auto end_insert = std::chrono::high_resolution_clock::now();
    auto insert_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_insert - start_insert);
    std::cout << "Insertion completed!" << std::endl;
    
    // Phase 2: Random Fetch with local/remote tracking
    std::cout << "\nFetching " << NUM_OPERATIONS << " key-value pairs randomly..." << std::endl;
    
    // Setup random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(1, NUM_OPERATIONS);
    
    std::vector<double> fetch_times;
    std::vector<double> local_fetch_times;
    std::vector<double> remote_fetch_times;
    fetch_times.reserve(NUM_OPERATIONS);
    local_fetch_times.reserve(NUM_OPERATIONS);
    remote_fetch_times.reserve(NUM_OPERATIONS);
    
    auto start_fetch_all = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        try {
            int random_key = key_dist(gen);
            
            // Check if key exists locally based on hash distribution
            bool is_local = isKeyLocal(random_key, LOCAL_NODE_ID, TOTAL_NODES);
            
            auto start_single_fetch = std::chrono::high_resolution_clock::now();
            std::string value = distributor.get(random_key);
            auto end_single_fetch = std::chrono::high_resolution_clock::now();
            
            auto single_fetch_duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_single_fetch - start_single_fetch);
            double fetch_time_ms = static_cast<double>(single_fetch_duration.count()) / 1000.0;
            
            fetch_times.push_back(fetch_time_ms);
            
            if (is_local) {
                local_fetch_times.push_back(fetch_time_ms);
            } else {
                remote_fetch_times.push_back(fetch_time_ms);
            }
            
        } catch (const std::exception& e) {
            std::cout << "Fetch error for random key: " << e.what() << std::endl;
        }
    }
    auto end_fetch_all = std::chrono::high_resolution_clock::now();
    auto total_fetch_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_fetch_all - start_fetch_all);
    
    // Calculate overall statistics
    if (!fetch_times.empty()) {
        double total_time = std::accumulate(fetch_times.begin(), fetch_times.end(), 0.0);
        double avg_time = total_time / fetch_times.size();
        double min_time = *std::min_element(fetch_times.begin(), fetch_times.end());
        double max_time = *std::max_element(fetch_times.begin(), fetch_times.end());
        
        std::cout << "\n=== RANDOM BENCHMARK RESULTS ===" << std::endl;
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "--- INSERTION METRICS ---" << std::endl;
        std::cout << "Total insertion time: " << insert_duration.count() << " ms" << std::endl;
        std::cout << "Average insertion time per operation: " <<
                     static_cast<double>(insert_duration.count()) / NUM_OPERATIONS << " ms" << std::endl;
        
        std::cout << "--- OVERALL FETCH METRICS ---" << std::endl;
        std::cout << "Total fetch time: " << total_fetch_duration.count() << " ms" << std::endl;
        std::cout << "Average fetch time: " << avg_time << " ms" << std::endl;
        std::cout << "Minimum fetch time: " << min_time << " ms" << std::endl;
        std::cout << "Maximum fetch time: " << max_time << " ms" << std::endl;
        std::cout << "Successful fetches: " << fetch_times.size() << "/" << NUM_OPERATIONS << std::endl;
        
        // Local fetch statistics
        if (!local_fetch_times.empty()) {
            double local_total = std::accumulate(local_fetch_times.begin(), local_fetch_times.end(), 0.0);
            double local_avg = local_total / local_fetch_times.size();
            double local_min = *std::min_element(local_fetch_times.begin(), local_fetch_times.end());
            double local_max = *std::max_element(local_fetch_times.begin(), local_fetch_times.end());
            
            std::cout << "--- LOCAL FETCH METRICS ---" << std::endl;
            std::cout << "Local fetches count: " << local_fetch_times.size() << std::endl;
            std::cout << "Local fetch percentage: " << 
                         (static_cast<double>(local_fetch_times.size()) / fetch_times.size()) * 100.0 << "%" << std::endl;
            std::cout << "Average local fetch time: " << local_avg << " ms" << std::endl;
            std::cout << "Minimum local fetch time: " << local_min << " ms" << std::endl;
            std::cout << "Maximum local fetch time: " << local_max << " ms" << std::endl;
            std::cout << "Total local fetch time: " << local_total << " ms" << std::endl;
        } else {
            std::cout << "--- LOCAL FETCH METRICS ---" << std::endl;
            std::cout << "No local fetches performed" << std::endl;
        }
        
        // Remote fetch statistics
        if (!remote_fetch_times.empty()) {
            double remote_total = std::accumulate(remote_fetch_times.begin(), remote_fetch_times.end(), 0.0);
            double remote_avg = remote_total / remote_fetch_times.size();
            double remote_min = *std::min_element(remote_fetch_times.begin(), remote_fetch_times.end());
            double remote_max = *std::max_element(remote_fetch_times.begin(), remote_fetch_times.end());
            
            std::cout << "--- REMOTE FETCH METRICS ---" << std::endl;
            std::cout << "Remote fetches count: " << remote_fetch_times.size() << std::endl;
            std::cout << "Remote fetch percentage: " << 
                         (static_cast<double>(remote_fetch_times.size()) / fetch_times.size()) * 100.0 << "%" << std::endl;
            std::cout << "Average remote fetch time: " << remote_avg << " ms" << std::endl;
            std::cout << "Minimum remote fetch time: " << remote_min << " ms" << std::endl;
            std::cout << "Maximum remote fetch time: " << remote_max << " ms" << std::endl;
            std::cout << "Total remote fetch time: " << remote_total << " ms" << std::endl;
        } else {
            std::cout << "--- REMOTE FETCH METRICS ---" << std::endl;
            std::cout << "No remote fetches performed" << std::endl;
        }
    }
}



int main(int argc, char** argv) {
    std::cout << "\nModulo-based Key-Value Store CLIENT" << std::endl;
    std::cout << "===================================" << std::endl;
    
    std::string mode;
    std::cout << "Enter server storage mode (memory/persistent): ";
    std::getline(std::cin, mode);
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    
    StorageMode storage_mode;
    if (mode == "persistent") {
        storage_mode = StorageMode::PERSISTENT;
    } else if (mode == "memory") {
        storage_mode = StorageMode::MEMORY;
    } else {
        std::cout << "Invalid storage mode. Defaulting to memory mode." << std::endl;
        storage_mode = StorageMode::MEMORY;
    }
    
    Config config = Config("../config/config.json");
    size_t mem_size = config.read_size();
    
    try {
        // CLIENT mode - connect to existing storage instead of creating new
        KvStore& kv_store = KvStore::get_instance(mem_size, storage_mode, ConnectionMode::CLIENT);
        
        std::cout << "Storage Mode: " << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT" : "IN-MEMORY") << std::endl;
        std::cout << "Connected to existing server storage successfully!" << std::endl;
        
        // Pass the local_store to ThalliumDistributor
        KVDistributor distributor(kv_store, config);
        
        printHelp();
        
        while (true) {
            std::string input;
            std::cout << "\n> ";
            std::getline(std::cin, input);
            
            if (input.empty()) continue;
            
            std::vector<std::string> args = parseCommand(input);
            std::string action = args[0];
            
            if (action == "exit") {
                break;
            } else if (action == "help") {
                printHelp();
            } else if (action == "put" && args.size() >= 3) {
                try {
                    int key = std::stoi(args[1]);
                    std::string value = args[2];
                    for (size_t i = 3; i < args.size(); i++) {
                        value += " " + args[i];
                    }
                    distributor.insert(key, value);
                    std::cout << "Put operation completed successfully" << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << std::endl;
                }
            } else if (action == "get" && args.size() >= 2) {
                try {
                    int key = std::stoi(args[1]);
                    std::string value = distributor.get(key);
                    std::cout << "Value: " << value << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << std::endl;
                }
            } else if (action == "update" && args.size() >= 3) {
                try {
                    int key = std::stoi(args[1]);
                    std::string value = args[2];
                    for (size_t i = 3; i < args.size(); i++) {
                        value += " " + args[i];
                    }
                    distributor.update(key, value);
                    std::cout << "Update operation completed successfully" << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << std::endl;
                }
            } else if (action == "delete" && args.size() >= 2) {
                try {
                    int key = std::stoi(args[1]);
                    distributor.deleteKey(key);
                    std::cout << "Delete operation completed successfully" << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << std::endl;
                }
            }else if (action == "benchmark") {
                std::cout << "Starting sequential benchmark..." << std::endl;
                try {
                    benchmark(distributor);
                } catch (const std::exception& e) {
                    std::cout << "Benchmark error: " << e.what() << std::endl;
                }
            } else if (action == "benchmark1") {
                std::cout << "Starting random benchmark..." << std::endl;
                try {
                    benchmark1(distributor);
                } catch (const std::exception& e) {
                    std::cout << "Benchmark1 error: " << e.what() << std::endl;
                }
            }  else {
                std::cout << "Unknown command. Type 'help' for available commands." << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "Failed to connect to server storage: " << e.what() << std::endl;
        std::cout << "Make sure the server is running before starting the client." << std::endl;
        return 1;
    }
    
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
