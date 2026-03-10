#ifndef KVDISTRIBUTOR_HPP
#define KVDISTRIBUTOR_HPP

#include "KVClient.hpp"
#include "KVStore.hpp"
#include "config.hpp"
#include <unordered_map>
#include <string>

class KVDistributor {
private:
    int count_of_node = 0;
    std::unordered_map<int, std::string> node_to_ip;
    std::string protocol;
    int local_node_id = 0;
    uint8_t provider_id = 0;
    KVClient kv_client;
    KvStore& kv;
    const Config& config;  // <-- added const reference to Config

    int getNodeId(int key);
    std::string getNodeToIP(int node_id);
    int getLocalNodeId();

public:
    KVDistributor(KvStore& kv_store, const Config& config);  // <-- constructor updated

    int getNodeCount();
    std::string get(int key);
    void insert(int key, const std::string& value);
    void update(int key, const std::string& value);
    void deleteKey(int key);
};

#endif // KVDISTRIBUTOR_HPP
