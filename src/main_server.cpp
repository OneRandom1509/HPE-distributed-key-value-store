#include "KVServer.hpp"
#include "KVStore.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <stdexcept>

// Helper function to parse memory size from string with unit suffix
std::size_t parseMemorySize(const std::string& size_str) {
    const std::size_t KB = 1024;
    const std::size_t MB = 1024 * 1024;
    const std::size_t GB = 1024 * 1024 * 1024;
    
    // Default to 100MB if parsing fails
    std::size_t size = 100 * MB;
    
    try {
        // Check if the string has a unit suffix (K/M/G)
        std::string num_part = size_str;
        std::transform(num_part.begin(), num_part.end(), num_part.begin(), ::toupper);
        
        char unit = 'M'; // Default to MB
        double num_value = 0;
        
        if (num_part.back() == 'K' || num_part.back() == 'M' || num_part.back() == 'G') {
            unit = num_part.back();
            num_part.pop_back();
            num_value = std::stod(num_part);
        } else {
            // If no unit specified, assume MB
            num_value = std::stod(num_part);
        }
        
        // Convert to bytes based on unit
        switch (unit) {
            case 'K': size = static_cast<std::size_t>(num_value * KB); break;
            case 'M': size = static_cast<std::size_t>(num_value * MB); break;
            case 'G': size = static_cast<std::size_t>(num_value * GB); break;
            default:  size = static_cast<std::size_t>(num_value * MB); break;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing memory size: " << e.what() << "\n";
        std::cerr << "Using default size of 100MB\n";
    }
    
    return size;
}

int main(int argc, char** argv) {
    // Default values
    std::string protocol = "ofi+tcp";
    int port = 8080;
    std::size_t mem_size = 100 * 1024 * 1024; // Default 100MB
    StorageMode storage_mode = StorageMode::MEMORY; // Default to memory mode
    
    // Debug: Print all arguments received
    std::cout << "\n=== SERVER STARTUP DEBUG ===\n";
    std::cout << "Arguments received: " << argc << std::endl;
    for (int i = 0; i < argc; i++) {
        std::cout << "argv[" << i << "]: " << argv[i] << std::endl;
    }
    std::cout << "============================\n";
    
    // Parse command line arguments
    if (argc >= 2) {
        protocol = argv[1];
    }
    
    if (argc >= 3) {
        try {
            port = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid port number. Using default: " << port << "\n";
        }
    }
    
    if (argc >= 4) {
        // Parse memory size with unit support
        mem_size = parseMemorySize(argv[3]);
    }
    
    // NEW: Parse storage mode argument (4th argument)
    if (argc >= 5) {
        std::string mode_arg = argv[4];
        std::transform(mode_arg.begin(), mode_arg.end(), mode_arg.begin(), ::tolower);
        
        if (mode_arg == "persistent") {
            storage_mode = StorageMode::PERSISTENT;
            std::cout << "[Server] Storage mode set to: PERSISTENT\n";
        } else if (mode_arg == "memory") {
            storage_mode = StorageMode::MEMORY;
            std::cout << "[Server] Storage mode set to: MEMORY\n";
        } else {
            std::cout << "[Server] Unknown storage mode '" << mode_arg << "', defaulting to MEMORY\n";
            storage_mode = StorageMode::MEMORY;
        }
    } else {
        std::cout << "[Server] No storage mode specified, defaulting to MEMORY\n";
    }
    
    // Get server IP address
    char ip_buffer[128];
    FILE* fp = popen("hostname -I | awk '{print $1}'", "r");
    fgets(ip_buffer, sizeof(ip_buffer), fp);
    pclose(fp);
    
    std::string address = protocol + "://" + std::string(ip_buffer) + ":" + std::to_string(port);
    address.erase(std::remove(address.begin(), address.end(), '\n'), address.end());
    
    uint16_t provider_id = 1;
    
    // Print startup configuration
    std::cout << "\n==== KV Server Configuration ====\n";
    std::cout << "Protocol:      " << protocol << "\n";
    std::cout << "Port:          " << port << "\n";
    std::cout << "Shared Memory: " << (mem_size / (1024 * 1024)) << "MB\n";
    std::cout << "Storage Mode:  " << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT" : "MEMORY") << "\n";
    std::cout << "Address:       " << address << "\n";
    if (storage_mode == StorageMode::PERSISTENT) {
        std::cout << "Data File:     ./kvstore_persistent.dat\n";
    }
    std::cout << "================================\n\n";
    
    // Start the Thallium engine
    tl::engine myEngine(address, THALLIUM_SERVER_MODE);
    std::cout << "Server running at " << myEngine.self() << std::endl;
    
    // Get hostname for logging
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::cout << "Server hostname: " << hostname << std::endl;
    
    // Initialize the KvStore with shared memory AND storage mode
    std::cout << "Initializing KvStore with " << (mem_size / (1024 * 1024)) << "MB in "
              << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT" : "MEMORY") << " mode\n";
    
    // CRITICAL CHANGE: Pass both memory size AND storage mode
    // In your server main function
    KvStore& kv = KvStore::get_instance(mem_size, storage_mode, ConnectionMode::SERVER);
    std::cout << "KvStore initialized successfully" << std::endl;
    
    // Create and start the KVServer
    KVServer server(myEngine, kv, provider_id);
    std::cout << "KVServer started with provider ID: " << provider_id << std::endl;
    std::cout << "Server is running. Connect using: " << myEngine.self() << std::endl;
    
    // Print final status
    std::cout << "\n=== SERVER READY ===\n";
    std::cout << "Storage: " << (storage_mode == StorageMode::PERSISTENT ? "Data will persist across restarts" : "Data will be lost on restart") << "\n";
    std::cout << "Endpoint: " << myEngine.self() << "\n";
    std::cout << "===================\n\n";
    
    // Wait for completion
    myEngine.wait_for_finalize();
    return 0;
}
