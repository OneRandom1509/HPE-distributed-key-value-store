#ifndef KVDISTRIBUTOR_HPP
#define KVDISTRIBUTOR_HPP

#include "KVClient.hpp"
#include "ConsistentHashRing.hpp"
#include "KVStore.hpp"
#include "config.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

class KVDistributor
{
public:
  KVDistributor(KvStore &kv_store, const Config &config);
  // Test constructor: allow injecting a mock IKVClient
  KVDistributor(KvStore &kv_store, const Config &config, IKVClient *client);

  int getNodeCount();
  void rebuildRing(const std::unordered_map<int, std::string> &node_endpoints,
                   int virtual_nodes_per_node = 2);
  bool fetchMembershipFromServer(const std::string &server_endpoint);
  std::string getFirstEndpoint() const;
  std::vector<std::string> getAllEndpoints() const;
  std::string get(int key);
  void insert(int key, const std::string &value);
  void update(int key, const std::string &value);
  void deleteKey(int key);

private:
  mutable std::mutex ring_mutex_;
  int count_of_node = 0;
  std::unordered_map<int, std::string> node_to_ip;
  std::string protocol;
  int local_node_id = 0;
  uint8_t provider_id = 0;
  IKVClient *kv_client;
  std::unique_ptr<KVClient> owned_kv_client;
  ConsistentHashRing hash_ring;
  KvStore &kv;
  const Config &config;

  std::string getNodeToIP(int node_id);
  int getLocalNodeId();
  bool isLocalNode(int node_id) const;
  void rebuildRing();
  bool readFromNode(int node_id, int key, std::string &value);
  enum class RemoteOp
  {
    INSERT,
    UPDATE
  };
  void
  writeToNode(int node_id, int key, const std::string &value, RemoteOp op);
  void deleteOnNode(int node_id, int key);
};

#endif // KVDISTRIBUTOR_HPP
