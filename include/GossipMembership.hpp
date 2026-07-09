#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <cstdint>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <thallium.hpp>

namespace tl = thallium;

enum class NodeStatus : uint8_t
{
  ALIVE = 0,
  SUSPECT = 1,
  DEAD = 2
};

struct MemberRecord
{
  int node_id;
  std::string endpoint;
  NodeStatus status;
  uint64_t heartbeat_counter;
  uint32_t generation;

  template <typename Archive> void serialize(Archive &ar)
  {
    ar & node_id;
    ar & endpoint;
    ar & status;
    ar & heartbeat_counter;
    ar & generation;
  }
};

using MembershipChangeCallback
  = std::function<void(const std::unordered_map<int, std::string> &)>;

class GossipMembership
{
public:
  GossipMembership(int self_node_id, const std::string &self_endpoint,
                   tl::engine &engine, uint16_t provider_id,
                   const std::vector<std::string> &seed_nodes,
                   int gossip_interval_ms, int suspect_threshold_ms,
                   int dead_threshold_ms);

  ~GossipMembership();

  void start();
  void stop();

  void setChangeCallback(MembershipChangeCallback cb);

  std::vector<MemberRecord>
  mergeView(const std::vector<MemberRecord> &incoming);

  std::vector<MemberRecord> getMemberView() const;

private:
  struct MemberEntry
  {
    MemberRecord record;
    uint64_t last_updated_ms;
  };

  void gossipLoop();
  void doGossipRound();
  std::vector<MemberRecord>
  sendGossipExchange(const std::string &peer_endpoint,
                     const std::vector<MemberRecord> &local_view);
  void detectFailures();
  std::string pickRandomPeer() const;
  void notifyChange();

  uint32_t loadGeneration();
  void saveGeneration(uint32_t gen);
  static uint64_t currentTimeMs();

  int self_node_id_;
  std::string self_endpoint_;
  uint16_t provider_id_;
  std::vector<std::string> seed_nodes_;
  int gossip_interval_ms_;
  int suspect_threshold_ms_;
  int dead_threshold_ms_;

  std::unordered_map<int, MemberEntry> member_table_;
  mutable std::mutex mutex_;

  tl::engine &engine_;
  tl::remote_procedure gossip_exchange_rpc_;

  std::atomic<bool> running_{false};
  std::thread gossip_thread_;

  MembershipChangeCallback change_callback_;

  std::set<int> last_alive_set_;

  uint32_t generation_;
};
