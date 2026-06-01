#include "KVDistributor.hpp"
#include "MurmurHash3.hpp"
#include <spdlog/spdlog.h>
#include <iostream>
#include <utility>

namespace
{
  uint32_t keyHash(int key)
  {
    const std::string key_str = std::to_string(key);
    return murmur3_32(key_str.data(), static_cast<int>(key_str.size()), 0);
  }

  bool isMissOrError(const std::string &value)
  {
    return value == "key not found" || value == "Map not found"
           || value.starts_with("Error:")
           || value.starts_with("Fetch operation failed:");
  }
}

KVDistributor::KVDistributor(KvStore &kv_store, const Config &config)
    : count_of_node(config.read_count()), node_to_ip(),
      protocol(config.read_protocol()), provider_id(1), kv_client(nullptr),
      owned_kv_client(), hash_ring(), kv(kv_store), config(config)
{
  // std::cout<<protocol<<"  "<<provider_id<<'\n';
  for(int i = 0; i < count_of_node; ++i)
    {
      std::string endpoint = config.get_endpoint(i);
      node_to_ip[i] = endpoint;
    }

  count_of_node = static_cast<int>(node_to_ip.size());

  local_node_id = getLocalNodeId();
  rebuildRing();

  // default client
  owned_kv_client = std::make_unique<KVClient>(protocol, 1);
  kv_client = owned_kv_client.get();
}

KVDistributor::KVDistributor(KvStore &kv_store, const Config &config,
                             IKVClient *client)
    : count_of_node(config.read_count()), node_to_ip(),
      protocol(config.read_protocol()), provider_id(1), kv_client(client),
      owned_kv_client(), hash_ring(), kv(kv_store), config(config)
{
  for(int i = 0; i < count_of_node; ++i)
    {
      std::string endpoint = config.get_endpoint(i);
      node_to_ip[i] = endpoint;
    }

  count_of_node = static_cast<int>(node_to_ip.size());

  local_node_id = getLocalNodeId();
  rebuildRing();
}

std::string KVDistributor::getNodeToIP(int node_id)
{
  return node_to_ip[node_id];
}

bool KVDistributor::isLocalNode(int node_id) const
{
  return node_id == local_node_id;
}

int KVDistributor::getLocalNodeId()
{
  std::string local_ip = config.read_ip();
  for(const auto &pair : node_to_ip)
    {
      if(pair.second == local_ip)
        {
          return pair.first;
        }
    }
  // If local_ip does not match any configured node endpoints,
  // return -1 to indicate there is no local node for this process.
  // This avoids accidentally treating the distributor as "local"
  // and performing writes against an in-process KvStore when the
  // process is actually a client only.
  return -1;
}

void KVDistributor::rebuildRing() { hash_ring.rebuild(node_to_ip); }

void KVDistributor::rebuildRing(
  const std::unordered_map<int, std::string> &node_endpoints,
  int virtual_nodes_per_node)
{
  node_to_ip = node_endpoints;
  count_of_node = static_cast<int>(node_to_ip.size());
  hash_ring.rebuild(node_to_ip, virtual_nodes_per_node);
}

int KVDistributor::getNodeCount() { return count_of_node; }

bool KVDistributor::readFromNode(int node_id, int key, std::string &value)
{
  if(node_id < 0)
    return false;

  const uint32_t hash = keyHash(key);
  spdlog::debug(
    "KVDistributor read key={} hash={} node={} endpoint={} local={}", key,
    hash, node_id, getNodeToIP(node_id), isLocalNode(node_id));

  if(isLocalNode(node_id))
    {
      value = kv.Find(key);
      return !isMissOrError(value);
    }

  try
    {
      std::string ep = getNodeToIP(node_id);
      value = kv_client->fetch(key, ep);
      return !isMissOrError(value);
    }
  catch(const std::exception &e)
    {
      std::cerr << "Error fetching key " << key << " from node " << node_id
                << ": " << e.what() << std::endl;
      return false;
    }
}

void KVDistributor::writeToNode(int node_id, int key, const std::string &value,
                                RemoteOp op)
{
  if(node_id < 0)
    return;

  const uint32_t hash = keyHash(key);
  const char *op_name = (op == RemoteOp::INSERT) ? "insert" : "update";
  spdlog::debug("KVDistributor {} key={} hash={} node={} endpoint={} local={}",
                op_name, key, hash, node_id, getNodeToIP(node_id),
                isLocalNode(node_id));

  if(isLocalNode(node_id))
    {
      try
        {
          if(op == RemoteOp::INSERT)
            kv.Insert(key, value);
          else if(op == RemoteOp::UPDATE)
            kv.Update(key, value);
        }
      catch(const std::exception &e)
        {
          std::cerr << "Error writing key " << key << " locally on node "
                    << node_id << ": " << e.what() << std::endl;
        }
      return;
    }

  try
    {
      std::string ep = getNodeToIP(node_id);
      if(kv_client)
        {
          if(op == RemoteOp::INSERT)
            kv_client->insert(key, value, ep);
          else if(op == RemoteOp::UPDATE)
            kv_client->update(key, value, ep);
        }
    }
  catch(const std::exception &e)
    {
      std::cerr << "Error writing key " << key << " to node " << node_id
                << ": " << e.what() << std::endl;
    }
}

void KVDistributor::deleteOnNode(int node_id, int key)
{
  if(node_id < 0)
    return;

  const uint32_t hash = keyHash(key);
  spdlog::debug(
    "KVDistributor delete key={} hash={} node={} endpoint={} local={}", key,
    hash, node_id, getNodeToIP(node_id), isLocalNode(node_id));

  if(isLocalNode(node_id))
    {
      try
        {
          kv.Delete(key);
        }
      catch(const std::exception &e)
        {
          std::cerr << "Error deleting key " << key << " locally on node "
                    << node_id << ": " << e.what() << std::endl;
        }
      return;
    }

  try
    {
      kv_client->deleteKey(key, getNodeToIP(node_id));
    }
  catch(const std::exception &e)
    {
      std::cerr << "Error deleting key " << key << " on node " << node_id
                << ": " << e.what() << std::endl;
    }
}

std::string KVDistributor::get(int key)
{
  std::string value;

  int primary_node_id = hash_ring.getPrimaryNode(key);
  int buddy_node_id = hash_ring.getBuddyNode(key);
  const uint32_t hash = keyHash(key);

  spdlog::debug("KVDistributor get key={} hash={} primary={} buddy={}", key,
                hash, primary_node_id, buddy_node_id);

  if(readFromNode(primary_node_id, key, value))
    {
      return value;
    }

  if(buddy_node_id != primary_node_id
     && readFromNode(buddy_node_id, key, value))
    {
      return value;
    }

  return "RPC Failed";
}

void KVDistributor::insert(int key, const std::string &value)
{
  int primary_node_id = hash_ring.getPrimaryNode(key);
  int buddy_node_id = hash_ring.getBuddyNode(key);
  const uint32_t hash = keyHash(key);

  spdlog::debug("KVDistributor insert key={} hash={} primary={} buddy={}", key,
                hash, primary_node_id, buddy_node_id);

  writeToNode(primary_node_id, key, value, RemoteOp::INSERT);
  if(buddy_node_id != primary_node_id)
    writeToNode(buddy_node_id, key, value, RemoteOp::INSERT);
}

void KVDistributor::update(int key, const std::string &value)
{
  int primary_node_id = hash_ring.getPrimaryNode(key);
  int buddy_node_id = hash_ring.getBuddyNode(key);
  const uint32_t hash = keyHash(key);

  spdlog::debug("KVDistributor update key={} hash={} primary={} buddy={}", key,
                hash, primary_node_id, buddy_node_id);

  writeToNode(primary_node_id, key, value, RemoteOp::UPDATE);
  if(buddy_node_id != primary_node_id)
    writeToNode(buddy_node_id, key, value, RemoteOp::UPDATE);
}

void KVDistributor::deleteKey(int key)
{
  int primary_node_id = hash_ring.getPrimaryNode(key);
  int buddy_node_id = hash_ring.getBuddyNode(key);
  const uint32_t hash = keyHash(key);

  spdlog::debug("KVDistributor delete key={} hash={} primary={} buddy={}", key,
                hash, primary_node_id, buddy_node_id);

  deleteOnNode(primary_node_id, key);
  if(buddy_node_id != primary_node_id)
    deleteOnNode(buddy_node_id, key);
}
