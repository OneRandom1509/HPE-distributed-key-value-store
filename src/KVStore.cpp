#include "KVStore.hpp"
#include <filesystem>
#include <iomanip>
#include <sstream>

// Static member definitions
const char* KvStore::MUTEX_NAME = "SharedMapMutex";
const std::string KvStore::PERSISTENT_FILE_PATH = "./kvstore_persistent.dat";

// Memory size constants
const std::size_t MB = 1024 * 1024;
const std::size_t DEFAULT_MEMORY_SIZE = 500 * MB;

// Modified get_instance method
KvStore& KvStore::get_instance(std::size_t size, StorageMode mode, ConnectionMode conn_mode) {
    std::cout << "[get_instance] Initializing KvStore with " << (size / MB) << "MB in "
              << (mode == StorageMode::PERSISTENT ? "PERSISTENT" : "MEMORY") << " mode as "
              << (conn_mode == ConnectionMode::SERVER ? "SERVER" : "CLIENT") << "\n";
    static KvStore instance(size > 0 ? size : DEFAULT_MEMORY_SIZE, mode, conn_mode);
    return instance;
}


// Modified constructor
KvStore::KvStore(std::size_t size, StorageMode mode, ConnectionMode conn_mode)
    : total_memory_size(size), storage_mode(mode), conn_mode(conn_mode),
      memory_map_ptr(nullptr), persistent_map_ptr(nullptr) {
    
    std::cout << "[KvStore] Constructor started with " << (size / MB) << "MB allocation in "
              << (mode == StorageMode::PERSISTENT ? "PERSISTENT" : "MEMORY") << " mode as "
              << (conn_mode == ConnectionMode::SERVER ? "SERVER" : "CLIENT") << "\n";
    
    try {
        if (mode == StorageMode::MEMORY) {
            if (conn_mode == ConnectionMode::SERVER) {
                createMemoryStorage(size);
            } else {
                connectToMemoryStorage();
            }
        } else {
            if (conn_mode == ConnectionMode::SERVER) {
                createPersistentStorage(size);
            } else {
                connectToPersistentStorage();
            }
        }
        
        // Handle mutex
        if (conn_mode == ConnectionMode::SERVER) {
            named_mutex::remove(MUTEX_NAME);
        }
        named_mutex mutex(open_or_create, MUTEX_NAME);
        std::cout << "[KvStore] Mutex " << (conn_mode == ConnectionMode::SERVER ? "created" : "connected") << " successfully\n";
        
        // Print initial memory stats
        PrintMemoryStats(conn_mode == ConnectionMode::SERVER ? "Server Initialization" : "Client Connection");
        
    } catch (const interprocess_exception& e) {
        std::cout << "[KvStore] Caught interprocess exception: " << e.what() << std::endl;
        
        if (conn_mode == ConnectionMode::CLIENT) {
            std::cout << "[KvStore] Client failed to connect to existing storage. Server might not be running.\n";
            throw;
        }
        
        // Server recovery logic (existing code)
        std::cout << "[KvStore] Server attempting recovery...\n";
        try {
            if (storage_mode == StorageMode::MEMORY) {
                memory_storage = std::make_unique<managed_shared_memory>(open_only, "Project");
                memory_map_ptr = memory_storage->find<MemoryHashMap>("SharedMap").first;
            } else {
                file_storage = std::make_unique<managed_mapped_file>(open_only, PERSISTENT_FILE_PATH.c_str());
                persistent_map_ptr = file_storage->find<MappedHashMap>("SharedMap").first;
            }
            std::cout << "[KvStore] Opened existing storage\n";
            PrintMemoryStats("Opened Existing");
        } catch (const interprocess_exception& e2) {
            std::cout << "[KvStore] Failed to open existing storage: " << e2.what() << std::endl;
            std::cout << "[KvStore] Creating new storage after cleanup\n";
            
            cleanupStorage();
            named_mutex::remove(MUTEX_NAME);
            
            if (storage_mode == StorageMode::MEMORY) {
                createMemoryStorage(size);
            } else {
                createPersistentStorage(size);
            }
            
            named_mutex mutex(open_or_create, MUTEX_NAME);
            std::cout << "[KvStore] Recovery successful\n";
            PrintMemoryStats("Recovery");
        }
    }
    
    std::cout << "[KvStore] Constructor finished\n";
}

void KvStore::createMemoryStorage(std::size_t size) {
    // Clean up any existing shared memory segments first
    try {
        managed_shared_memory existing_mem(open_only, "Project");
        auto result = existing_mem.find<MemoryHashMap>("SharedMap");
        if (result.first != nullptr) {
            std::cout << "[KvStore] Found existing shared memory with "
                      << result.first->size() << " entries, will remove and recreate\n";
        }
    } catch (const interprocess_exception& e) {
        std::cout << "[KvStore] No existing shared memory found (expected): " << e.what() << std::endl;
    }
    
    // Remove any existing shared memory with the same name
    shared_memory_object::remove("Project");
    
    // Create new shared memory
    try {
        memory_storage = std::make_unique<managed_shared_memory>(create_only, "Project", size);
        std::cout << "[KvStore] Created new shared memory segment: " << (size / (1024 * 1024)) << "MB\n";
        
        // Construct the unordered map in shared memory
        MemoryMapAllocator allocator(memory_storage->get_segment_manager());
        memory_map_ptr = memory_storage->construct<MemoryHashMap>("SharedMap")(
            0,
            boost::hash<int>(),
            std::equal_to<int>(),
            allocator
        );
        
        std::cout << "[KvStore] Created new memory-based unordered map\n";
        
    } catch (const interprocess_exception& e) {
        std::cout << "[KvStore] Failed to create shared memory: " << e.what() << std::endl;
        throw;
    }
}

void KvStore::createPersistentStorage(std::size_t size) {
    bool file_exists = std::filesystem::exists(PERSISTENT_FILE_PATH);
    std::cout << "[KvStore] Persistent file exists: " << (file_exists ? "YES" : "NO") << std::endl;
    
    if (file_exists) {
        try {
            // Try to open existing file
            file_storage = std::make_unique<managed_mapped_file>(open_only, PERSISTENT_FILE_PATH.c_str());
            
            // Try to find existing map
            auto result = file_storage->find<MappedHashMap>("SharedMap");
            if (result.first != nullptr) {
                persistent_map_ptr = result.first;
                std::cout << "[KvStore] Successfully opened existing persistent storage with "
                          << persistent_map_ptr->size() << " entries\n";
                return;
            } else {
                std::cout << "[KvStore] File exists but no valid map found, will recreate\n";
                file_storage.reset();
            }
        } catch (const interprocess_exception& e) {
            std::cout << "[KvStore] Failed to open existing file: " << e.what() << std::endl;
            file_storage.reset();
        }
    }
    
    // Remove existing file if corrupted or doesn't exist
    if (file_exists) {
        std::filesystem::remove(PERSISTENT_FILE_PATH);
        std::cout << "[KvStore] Removed existing corrupted file\n";
    }
    
    // Create new memory-mapped file
    try {
        file_storage = std::make_unique<managed_mapped_file>(create_only, PERSISTENT_FILE_PATH.c_str(), size);
        std::cout << "[KvStore] Created new memory-mapped file: " << PERSISTENT_FILE_PATH << "\n";
        
        // Construct the unordered map in mapped file
        MappedMapAllocator map_allocator(file_storage->get_segment_manager());
        persistent_map_ptr = file_storage->construct<MappedHashMap>("SharedMap")(
            0,
            boost::hash<int>(),
            std::equal_to<int>(),
            map_allocator
        );
        
        std::cout << "[KvStore] Created new persistent unordered map\n";
        
        // Immediate sync to ensure file is written
        file_storage->flush();
        
    } catch (const interprocess_exception& e) {
        std::cout << "[KvStore] Failed to create persistent storage: " << e.what() << std::endl;
        throw;
    }
}


// New method to connect to existing memory storage
void KvStore::connectToMemoryStorage() {
    try {
        std::cout << "[KvStore] Attempting to connect to existing shared memory...\n";
        memory_storage = std::make_unique<managed_shared_memory>(open_only, "Project");
        
        auto result = memory_storage->find<MemoryHashMap>("SharedMap");
        if (result.first != nullptr) {
            memory_map_ptr = result.first;
            std::cout << "[KvStore] Successfully connected to existing shared memory with "
                      << memory_map_ptr->size() << " entries\n";
        } else {
            throw interprocess_exception("SharedMap not found in existing shared memory");
        }
    } catch (const interprocess_exception& e) {
        std::cout << "[KvStore] Failed to connect to existing shared memory: " << e.what() << std::endl;
        throw;
    }
}

// New method to connect to existing persistent storage
void KvStore::connectToPersistentStorage() {
    try {
        std::cout << "[KvStore] Attempting to connect to existing persistent storage...\n";
        
        if (!std::filesystem::exists(PERSISTENT_FILE_PATH)) {
            throw interprocess_exception("Persistent file does not exist");
        }
        
        file_storage = std::make_unique<managed_mapped_file>(open_only, PERSISTENT_FILE_PATH.c_str());
        
        auto result = file_storage->find<MappedHashMap>("SharedMap");
        if (result.first != nullptr) {
            persistent_map_ptr = result.first;
            std::cout << "[KvStore] Successfully connected to existing persistent storage with "
                      << persistent_map_ptr->size() << " entries\n";
        } else {
            throw interprocess_exception("SharedMap not found in existing persistent storage");
        }
    } catch (const interprocess_exception& e) {
        std::cout << "[KvStore] Failed to connect to existing persistent storage: " << e.what() << std::endl;
        throw;
    }
}


void KvStore::cleanupStorage() {
    if (storage_mode == StorageMode::MEMORY) {
        shared_memory_object::remove("Project");
        
        // Clean up additional files for memory mode
        try {
            std::vector<std::string> files_to_remove = {
                "mappings.txt",
                "local_store_node_0.dat"
            };
            
            for (const auto& filename : files_to_remove) {
                if (std::filesystem::exists(filename)) {
                    std::filesystem::remove(filename);
                    std::cout << "[KvStore] Removed " << filename << "\n";
                }
            }
            
            // Remove pattern files: local_store_node_*.dat
            for (const auto& entry : std::filesystem::directory_iterator(".")) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.find("local_store_node_") == 0 && filename.ends_with(".dat")) {
                        std::filesystem::remove(entry.path());
                        std::cout << "[KvStore] Removed " << filename << "\n";
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cout << "[KvStore] Error cleaning up files: " << e.what() << "\n";
        }
    } else {
        if (std::filesystem::exists(PERSISTENT_FILE_PATH)) {
            std::filesystem::remove(PERSISTENT_FILE_PATH);
        }
    }
}

void KvStore::Sync() {
    if (storage_mode == StorageMode::PERSISTENT && file_storage) {
        file_storage->flush();
        std::cout << "[KvStore] Data synchronized to disk\n";
    } else if (storage_mode == StorageMode::MEMORY) {
        std::cout << "[KvStore] Sync called but storage is in-memory mode\n";
    }
}

MemoryStats KvStore::GetMemoryStats() const {
    MemoryStats stats = {0, 0, 0, 0.0};
    
    if (storage_mode == StorageMode::MEMORY && memory_storage) {
        stats.total_size = total_memory_size;
        stats.free_memory = memory_storage->get_free_memory();
        stats.used_memory = stats.total_size - stats.free_memory;
        stats.usage_percent = (static_cast<double>(stats.used_memory) / stats.total_size) * 100.0;
    } else if (storage_mode == StorageMode::PERSISTENT && file_storage) {
        stats.total_size = total_memory_size;
        stats.free_memory = file_storage->get_free_memory();
        stats.used_memory = stats.total_size - stats.free_memory;
        stats.usage_percent = (static_cast<double>(stats.used_memory) / stats.total_size) * 100.0;
    }
    
    return stats;
}

void KvStore::PrintMemoryStats(const std::string& operation) const {
    MemoryStats stats = GetMemoryStats();
    std::cout << "\n========== MEMORY STATS [" << operation << "] ==========\n";
    std::cout << "  Storage mode: " << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT" : "MEMORY") << "\n";
    std::cout << "  Total memory: " << std::fixed << std::setprecision(2)
              << (static_cast<double>(stats.total_size) / MB) << " MB\n";
    std::cout << "  Used memory:  " << std::fixed << std::setprecision(2)
              << (static_cast<double>(stats.used_memory) / MB) << " MB\n";
    std::cout << "  Free memory:  " << std::fixed << std::setprecision(2)
              << (static_cast<double>(stats.free_memory) / MB) << " MB\n";
    std::cout << "  Usage:        " << std::fixed << std::setprecision(2)
              << stats.usage_percent << "%\n";
    if (storage_mode == StorageMode::PERSISTENT) {
        std::cout << "  File path:    " << PERSISTENT_FILE_PATH << "\n";
    }
    std::cout << "============================================\n";
}

std::size_t KvStore::GetFreeMemory() const {
    if (storage_mode == StorageMode::MEMORY && memory_storage) {
        return memory_storage->get_free_memory();
    } else if (storage_mode == StorageMode::PERSISTENT && file_storage) {
        return file_storage->get_free_memory();
    }
    return 0;
}

std::size_t KvStore::GetUsedMemory() const {
    return total_memory_size - GetFreeMemory();
}

bool KvStore::hasEnoughMemory(std::size_t needed_bytes) const {
    const std::size_t OVERHEAD_FACTOR = 2; // More conservative overhead
    std::size_t estimated_need = needed_bytes * OVERHEAD_FACTOR;
    std::size_t free_memory = GetFreeMemory();
    return free_memory >= estimated_need;
}

void KvStore::Insert(int key, const std::string& value) {
    try {
        if ((storage_mode == StorageMode::MEMORY && !memory_map_ptr) ||
            (storage_mode == StorageMode::PERSISTENT && !persistent_map_ptr)) {
            std::cout << "Map not found in storage." << std::endl;
            return;
        }
        
        named_mutex mutex(open_only, MUTEX_NAME);
        scoped_lock<named_mutex> lock(mutex);
        
        // Check memory requirements first
        std::size_t entry_size = value.size() + sizeof(int) + 64; // Added overhead estimate
        if (!hasEnoughMemory(entry_size)) {
            std::cout << "Error: Not enough memory for insertion.\n";
            PrintMemoryStats("Insert - Failed (Memory)");
            return;
        }
        
        if (storage_mode == StorageMode::MEMORY) {
            // Check if key already exists
            auto it = memory_map_ptr->find(key);
            if (it != memory_map_ptr->end()) {
                std::cout << "Key " << key << " already exists. Use update instead.\n";
                PrintMemoryStats("Insert - Key Exists");
                return;
            }
            
            CharAllocator char_allocator(memory_storage->get_segment_manager());
            memory_map_ptr->insert(std::make_pair(key, MyShmString(value.c_str(), char_allocator)));
            
        } else {
            // Check if key already exists
            auto it = persistent_map_ptr->find(key);
            if (it != persistent_map_ptr->end()) {
                std::cout << "Key " << key << " already exists. Use update instead.\n";
                PrintMemoryStats("Insert - Key Exists");
                return;
            }
            
            MappedCharAllocator char_allocator(file_storage->get_segment_manager());
            persistent_map_ptr->insert(std::make_pair(key, MappedShmString(value.c_str(), char_allocator)));
            
            // Sync to disk for persistence
            Sync();
        }
        
        std::cout << "Inserted key " << key << " with value: " << value << std::endl;
        PrintMemoryStats("After Insert");
        
    } catch (const boost::interprocess::bad_alloc& e) {
        std::cout << "Error during insertion (bad_alloc): " << e.what() << std::endl;
        PrintMemoryStats("Insert - Failed (Allocation)");
    } catch (const std::exception& e) {
        std::cout << "Error during insertion: " << e.what() << std::endl;
    }
}

void KvStore::Update(int key, const std::string& new_value) {
    try {
        if ((storage_mode == StorageMode::MEMORY && !memory_map_ptr) ||
            (storage_mode == StorageMode::PERSISTENT && !persistent_map_ptr)) {
            std::cout << "Map not found in storage." << std::endl;
            return;
        }
        
        named_mutex mutex(open_only, MUTEX_NAME);
        scoped_lock<named_mutex> lock(mutex);
        
        if (storage_mode == StorageMode::MEMORY) {
            auto it = memory_map_ptr->find(key);
            if (it != memory_map_ptr->end()) {
                std::size_t old_size = std::string(it->second.c_str()).size();
                std::size_t new_size = new_value.size();
                std::size_t size_diff = new_size > old_size ? (new_size - old_size) : 0;
                
                if (size_diff > 0 && !hasEnoughMemory(size_diff)) {
                    std::cout << "Error: Not enough memory for update.\n";
                    PrintMemoryStats("Update - Failed (Memory)");
                    return;
                }
                
                CharAllocator char_alloc(memory_storage->get_segment_manager());
                MyShmString shm_string(new_value.c_str(), char_alloc);
                it->second = shm_string;
                
                std::cout << "Key " << key << " updated to: " << new_value << std::endl;
                PrintMemoryStats("After Update");
            } else {
                std::cout << "Key " << key << " not found. Cannot update." << std::endl;
                PrintMemoryStats("Update - Key Not Found");
            }
        } else {
            auto it = persistent_map_ptr->find(key);
            if (it != persistent_map_ptr->end()) {
                std::size_t old_size = std::string(it->second.c_str()).size();
                std::size_t new_size = new_value.size();
                std::size_t size_diff = new_size > old_size ? (new_size - old_size) : 0;
                
                if (size_diff > 0 && !hasEnoughMemory(size_diff)) {
                    std::cout << "Error: Not enough memory for update.\n";
                    PrintMemoryStats("Update - Failed (Memory)");
                    return;
                }
                
                MappedCharAllocator char_alloc(file_storage->get_segment_manager());
                MappedShmString shm_string(new_value.c_str(), char_alloc);
                it->second = shm_string;
                
                // Sync to disk for persistence
                Sync();
                
                std::cout << "Key " << key << " updated to: " << new_value << std::endl;
                PrintMemoryStats("After Update");
            } else {
                std::cout << "Key " << key << " not found. Cannot update." << std::endl;
                PrintMemoryStats("Update - Key Not Found");
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error during update: " << e.what() << std::endl;
    }
}

void KvStore::Delete(int key) {
    try {
        if ((storage_mode == StorageMode::MEMORY && !memory_map_ptr) ||
            (storage_mode == StorageMode::PERSISTENT && !persistent_map_ptr)) {
            std::cout << "Map not found in storage." << std::endl;
            return;
        }
        
        named_mutex mutex(open_only, MUTEX_NAME);
        scoped_lock<named_mutex> lock(mutex);
        
        std::size_t pre_free_memory = GetFreeMemory();
        
        if (storage_mode == StorageMode::MEMORY) {
            auto it = memory_map_ptr->find(key);
            if (it != memory_map_ptr->end()) {
                std::cout << "Deleting key " << key << std::endl;
                memory_map_ptr->erase(it);
                
                std::size_t post_free_memory = GetFreeMemory();
                std::size_t memory_freed = post_free_memory - pre_free_memory;
                std::cout << "Key " << key << " deleted. Freed approximately "
                          << memory_freed << " bytes." << std::endl;
                PrintMemoryStats("After Delete");
            } else {
                std::cout << "Key " << key << " not found. Nothing to delete." << std::endl;
                PrintMemoryStats("Delete - Key Not Found");
            }
        } else {
            auto it = persistent_map_ptr->find(key);
            if (it != persistent_map_ptr->end()) {
                std::cout << "Deleting key " << key << std::endl;
                persistent_map_ptr->erase(it);
                
                // Sync to disk for persistence
                Sync();
                
                std::size_t post_free_memory = GetFreeMemory();
                std::size_t memory_freed = post_free_memory - pre_free_memory;
                std::cout << "Key " << key << " deleted. Freed approximately "
                          << memory_freed << " bytes." << std::endl;
                PrintMemoryStats("After Delete");
            } else {
                std::cout << "Key " << key << " not found. Nothing to delete." << std::endl;
                PrintMemoryStats("Delete - Key Not Found");
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error during delete: " << e.what() << std::endl;
    }
}

std::string KvStore::Find(int key) {
    try {
        if ((storage_mode == StorageMode::MEMORY && !memory_map_ptr) ||
            (storage_mode == StorageMode::PERSISTENT && !persistent_map_ptr)) {
            std::cout << "Map not found in storage." << std::endl;
            return "Map not found";
        }
        
        named_mutex mutex(open_only, MUTEX_NAME);
        scoped_lock<named_mutex> lock(mutex);
        
        if (storage_mode == StorageMode::MEMORY) {
            auto it = memory_map_ptr->find(key);
            if (it != memory_map_ptr->end()) {
                std::string normal_str = std::string(it->second.c_str());
                std::cout << "Found key " << key << " with value: " << normal_str << std::endl;
                PrintMemoryStats("After Find");
                return normal_str;
            } else {
                std::cout << "Key " << key << " not found in storage." << std::endl;
                PrintMemoryStats("Find - Key Not Found");
                return "key not found";
            }
        } else {
            auto it = persistent_map_ptr->find(key);
            if (it != persistent_map_ptr->end()) {
                std::string normal_str = std::string(it->second.c_str());
                std::cout << "Found key " << key << " with value: " << normal_str << std::endl;
                PrintMemoryStats("After Find");
                return normal_str;
            } else {
                std::cout << "Key " << key << " not found in storage." << std::endl;
                PrintMemoryStats("Find - Key Not Found");
                return "key not found";
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Error during find operation: " << e.what() << std::endl;
        return "Error: " + std::string(e.what());
    }
}

std::size_t KvStore::GetMapSize() const {
    try {
        if ((storage_mode == StorageMode::MEMORY && !memory_map_ptr) ||
            (storage_mode == StorageMode::PERSISTENT && !persistent_map_ptr)) {
            return 0;
        }
        
        named_mutex mutex(open_only, MUTEX_NAME);
        scoped_lock<named_mutex> lock(mutex);
        
        if (storage_mode == StorageMode::MEMORY) {
            return memory_map_ptr->size();
        } else {
            return persistent_map_ptr->size();
        }
    } catch (const std::exception& e) {
        std::cout << "Error getting map size: " << e.what() << std::endl;
        return 0;
    }
}

void KvStore::ListAllKeys() const {
    try {
        if ((storage_mode == StorageMode::MEMORY && !memory_map_ptr) ||
            (storage_mode == StorageMode::PERSISTENT && !persistent_map_ptr)) {
            std::cout << "Map not found in storage." << std::endl;
            return;
        }
        
        named_mutex mutex(open_only, MUTEX_NAME);
        scoped_lock<named_mutex> lock(mutex);
        
        std::cout << "\n========== ALL KEYS IN STORAGE ==========\n";
        
        if (storage_mode == StorageMode::MEMORY) {
            std::cout << "Total keys: " << memory_map_ptr->size() << "\n";
            for (const auto& pair : *memory_map_ptr) {
                std::cout << "Key: " << pair.first
                         << ", Value: " << std::string(pair.second.c_str()) << "\n";
            }
        } else {
            std::cout << "Total keys: " << persistent_map_ptr->size() << "\n";
            for (const auto& pair : *persistent_map_ptr) {
                std::cout << "Key: " << pair.first
                         << ", Value: " << std::string(pair.second.c_str()) << "\n";
            }
        }
        std::cout << "========================================\n";
    } catch (const std::exception& e) {
        std::cout << "Error listing keys: " << e.what() << std::endl;
    }
}

KvStore::~KvStore() {
    // Check if we're in client mode - if so, don't clean up shared resources
    if (conn_mode == ConnectionMode::CLIENT) {
        std::cout << "[KvStore] Client disconnecting, leaving shared resources intact" << std::endl;
        return;
    }

    std::cout << "[KvStore] Destructor called, cleaning up resources" << std::endl;
    
    try {
        // Final memory stats
        PrintMemoryStats("Before Destruction");
        
        if (storage_mode == StorageMode::MEMORY && memory_storage) {
            // Destroy unordered map object in shared memory
            if (memory_map_ptr) {
                memory_storage->destroy<MemoryHashMap>("SharedMap");
                std::cout << "Destroyed unordered map from shared memory." << std::endl;
                memory_map_ptr = nullptr;
            }
            
            // Clean up all memory mode files
            cleanupStorage();
            memory_storage.reset();
            
        } else if (storage_mode == StorageMode::PERSISTENT && file_storage) {
            // Destroy unordered map object in mapped file
            if (persistent_map_ptr) {
                // Final sync before destruction
                file_storage->flush();
                file_storage->destroy<MappedHashMap>("SharedMap");
                std::cout << "Destroyed persistent map from mapped file." << std::endl;
                persistent_map_ptr = nullptr;
}
            
            file_storage.reset();
            std::cout << "Persistent storage cleaned up." << std::endl;
        }
        
        // Clean up mutex
        try {
            named_mutex::remove(MUTEX_NAME);
            std::cout << "Mutex cleaned up." << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not clean up mutex: " << e.what() << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Error during cleanup: " << e.what() << std::endl;
    }
    
    std::cout << "[KvStore] Destructor completed" << std::endl;
}
