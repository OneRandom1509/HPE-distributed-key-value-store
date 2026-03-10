#ifndef KVSTORE_HPP
#define KVSTORE_HPP

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/unordered_map.hpp>
#include <iostream>
#include <string>
#include <memory>

using namespace boost::interprocess;

// Memory storage type definitions
typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef basic_string<char, std::char_traits<char>, CharAllocator> MyShmString;
typedef allocator<std::pair<const int, MyShmString>, managed_shared_memory::segment_manager> MemoryMapAllocator;
typedef boost::unordered_map<int, MyShmString, boost::hash<int>, std::equal_to<int>, MemoryMapAllocator> MemoryHashMap;

// Persistent storage type definitions
typedef allocator<char, managed_mapped_file::segment_manager> MappedCharAllocator;
typedef basic_string<char, std::char_traits<char>, MappedCharAllocator> MappedShmString;
typedef allocator<std::pair<const int, MappedShmString>, managed_mapped_file::segment_manager> MappedMapAllocator;
typedef boost::unordered_map<int, MappedShmString, boost::hash<int>, std::equal_to<int>, MappedMapAllocator> MappedHashMap;

enum class StorageMode {
    MEMORY,
    PERSISTENT
};

enum class ConnectionMode {
    SERVER,  // Create new storage
    CLIENT   // Connect to existing storage
};

struct MemoryStats {
    std::size_t total_size;
    std::size_t used_memory;
    std::size_t free_memory;
    double usage_percent;
};

class KvStore {
private:
    static const std::string PERSISTENT_FILE_PATH;
    static const char* MUTEX_NAME;
    
    std::size_t total_memory_size;
    StorageMode storage_mode;
    ConnectionMode conn_mode;  // Add this line
    
    // Type-safe storage pointers
    std::unique_ptr<managed_shared_memory> memory_storage;
    std::unique_ptr<managed_mapped_file> file_storage;
    
    // Map pointers (only one will be non-null based on storage mode)
    MemoryHashMap* memory_map_ptr;
    MappedHashMap* persistent_map_ptr;
    
    // Private constructor for singleton
    KvStore(std::size_t size, StorageMode mode, ConnectionMode conn_mode);
    
    // Helper methods
    void createMemoryStorage(std::size_t size);
    void createPersistentStorage(std::size_t size);
    void cleanupStorage();
    bool hasEnoughMemory(std::size_t needed_bytes) const;

    void connectToMemoryStorage();
    void connectToPersistentStorage();
    
public:
    // Singleton access
    static KvStore& get_instance(std::size_t size, StorageMode mode, ConnectionMode conn_mode = ConnectionMode::SERVER);
    
    // Disable copy constructor and assignment
    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;
    
    // Destructor
    ~KvStore();
    
    // Core operations
    void Insert(int key, const std::string& value);
    void Update(int key, const std::string& new_value);
    void Delete(int key);
    std::string Find(int key);
    
    // Utility operations
    void Sync();
    std::size_t GetMapSize() const;
    void ListAllKeys() const;
    
    // Memory management
    MemoryStats GetMemoryStats() const;
    void PrintMemoryStats(const std::string& operation) const;
    std::size_t GetFreeMemory() const;
    std::size_t GetUsedMemory() const;
};

#endif // KVSTORE_HPP
