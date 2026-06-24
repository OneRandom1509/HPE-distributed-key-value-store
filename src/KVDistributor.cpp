#include "KVDistributor.hpp"
#include "GossipMembership.hpp"
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

KVDistributor::KVDistributor(KvStore *kv_store, const Config &config)
    : node_to_ip(), protocol(config.read_protocol()), provider_id(1),
      kv_client(nullptr), owned_kv_client(), hash_ring(), kv(kv_store),
      config(config)
{
  std::string local_ip = config.read_ip();
  if(!local_ip.empty())
    {
      node_to_ip[0] = local_ip;
      count_of_node = 1;
    }

  local_node_id = getLocalNodeId();
  rebuildRing();

  owned_kv_client = std::make_unique<KVClient>(protocol, 1, 2000);
  kv_client = owned_kv_client.get();
}

KVDistributor::KVDistributor(KvStore *kv_store, const Config &config,
                             IKVClient *client)
    : node_to_ip(), protocol(config.read_protocol()), provider_id(1),
      kv_client(client), owned_kv_client(), hash_ring(), kv(kv_store),
      config(config)
{
  std::string local_ip = config.read_ip();
  if(!local_ip.empty())
    {
      node_to_ip[0] = local_ip;
      count_of_node = 1;
    }

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

void KVDistributor::rebuildRing()
{
  std::lock_guard<std::mutex> lock(ring_mutex_);
  hash_ring.rebuild(node_to_ip);
  local_node_id = getLocalNodeId();
}

void KVDistributor::rebuildRing(
  const std::unordered_map<int, std::string> &node_endpoints,
  int virtual_nodes_per_node)
{
  std::lock_guard<std::mutex> lock(ring_mutex_);
  node_to_ip = node_endpoints;
  count_of_node = static_cast<int>(node_to_ip.size());
  hash_ring.rebuild(node_to_ip, virtual_nodes_per_node);
  local_node_id = getLocalNodeId();
}

int KVDistributor::getNodeCount()
{
  std::lock_guard<std::mutex> lock(ring_mutex_);
  return count_of_node;
}

std::string KVDistributor::getFirstEndpoint() const
{
  std::lock_guard<std::mutex> lock(ring_mutex_);
  if(node_to_ip.empty())
    return {};
  return node_to_ip.begin()->second;
}

std::vector<std::string> KVDistributor::getAllEndpoints() const
{
  std::lock_guard<std::mutex> lock(ring_mutex_);
  std::vector<std::string> endpoints;
  endpoints.reserve(node_to_ip.size());
  for(const auto &[_, ep] : node_to_ip)
    endpoints.push_back(ep);
  return endpoints;
}

bool KVDistributor::fetchMembershipFromServer(
  const std::string &server_endpoint)
{
  try
    {
      auto members = kv_client->getMembership(server_endpoint);
      spdlog::debug("fetchMembershipFromServer: got {} member(s) from {}",
                    members.size(), server_endpoint);
      std::unordered_map<int, std::string> live;
      for(const auto &m : members)
        {
          if(m.status != NodeStatus::ALIVE)
            continue;
          std::string ep = m.endpoint;
          auto pos = ep.find("://");
          if(pos != std::string::npos)
            ep = ep.substr(pos + 3);
          live[m.node_id] = ep;
        }
      spdlog::debug(
        "fetchMembershipFromServer: {} ALIVE member(s) after filtering",
        live.size());

      if(live.empty())
        {
          // Log one member's status for debugging
          if(!members.empty())
            {
              auto &m = members.front();
              spdlog::warn("First member: id={} status={} endpoint={}",
                           m.node_id, static_cast<int>(m.status), m.endpoint);
            }
          return false;
        }
      std::lock_guard<std::mutex> lock(ring_mutex_);
      if(live == node_to_ip)
        return true;
      node_to_ip = live;
      count_of_node = static_cast<int>(node_to_ip.size());
      hash_ring.rebuild(node_to_ip);
      local_node_id = getLocalNodeId();
      return true;
    }
  catch(const std::exception &e)
    {
      std::cerr << "fetchMembershipFromServer failed: " << e.what()
                << std::endl;
      return false;
    }
}

bool KVDistributor::readFromNode(int node_id, int key, std::string &value)
{
  if(node_id < 0)
    return false;

  const uint32_t hash = keyHash(key);
  spdlog::debug(
    "KVDistributor read key={} hash={} node={} endpoint={} local={}", key,
    hash, node_id, getNodeToIP(node_id), isLocalNode(node_id));

  if(isLocalNode(node_id) && kv)
    {
      value = kv->Find(key);
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

  if(isLocalNode(node_id) && kv)
    {
      try
        {
          if(op == RemoteOp::INSERT)
            kv->Insert(key, value);
          else if(op == RemoteOp::UPDATE)
            kv->Update(key, value);
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

  if(isLocalNode(node_id) && kv)
    {
      try
        {
          kv->Delete(key);
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
  std::lock_guard<std::mutex> lock(ring_mutex_);
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
  std::lock_guard<std::mutex> lock(ring_mutex_);
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
  std::lock_guard<std::mutex> lock(ring_mutex_);
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
  std::lock_guard<std::mutex> lock(ring_mutex_);
  int primary_node_id = hash_ring.getPrimaryNode(key);
  int buddy_node_id = hash_ring.getBuddyNode(key);
  const uint32_t hash = keyHash(key);

  spdlog::debug("KVDistributor delete key={} hash={} primary={} buddy={}", key,
                hash, primary_node_id, buddy_node_id);

  deleteOnNode(primary_node_id, key);
  if(buddy_node_id != primary_node_id)
    deleteOnNode(buddy_node_id, key);
}
