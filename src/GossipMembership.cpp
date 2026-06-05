#include "GossipMembership.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <thallium/serialization/stl/vector.hpp>
#include <thallium/serialization/stl/string.hpp>

namespace
{
  uint64_t nowMs()
  {
    return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch())
        .count());
  }

  std::string generationFilePath(int node_id)
  {
    return "generation_" + std::to_string(node_id) + ".dat";
  }

  // Deterministic random peer selection seeded per round
  int randomInt(int exclusive_max)
  {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, exclusive_max - 1);
    return dist(gen);
  }
}

GossipMembership::GossipMembership(
  int self_node_id, const std::string &self_endpoint, tl::engine &engine,
  uint16_t provider_id, const std::vector<std::string> &seed_nodes,
  int gossip_interval_ms, int suspect_threshold_ms, int dead_threshold_ms)
    : self_node_id_(self_node_id), self_endpoint_(self_endpoint),
      provider_id_(provider_id), seed_nodes_(seed_nodes),
      gossip_interval_ms_(gossip_interval_ms),
      suspect_threshold_ms_(suspect_threshold_ms),
      dead_threshold_ms_(dead_threshold_ms), engine_(engine),
      gossip_exchange_rpc_(engine.define("gossip_exchange"))
{
  generation_ = loadGeneration() + 1;
  saveGeneration(generation_);

  MemberEntry self_entry;
  self_entry.record.node_id = self_node_id_;
  self_entry.record.endpoint = self_endpoint_;
  self_entry.record.status = NodeStatus::ALIVE;
  self_entry.record.heartbeat_counter = 0;
  self_entry.record.generation = generation_;
  self_entry.last_updated_ms = nowMs();
  member_table_[self_node_id_] = self_entry;
}

GossipMembership::~GossipMembership() { stop(); }

void GossipMembership::start()
{
  running_ = true;

  // Contact seed nodes first to bootstrap view
  for(const auto &seed : seed_nodes_)
    {
      if(seed == self_endpoint_)
        continue;
      try
        {
          std::vector<MemberRecord> local;
          {
            std::lock_guard<std::mutex> lock(mutex_);
            for(const auto &[_, e] : member_table_)
              local.push_back(e.record);
          }
          auto response = sendGossipExchange(seed, local);
          std::lock_guard<std::mutex> lock(mutex_);
          for(const auto &r : response)
            {
              auto &entry = member_table_[r.node_id];
              if(r.node_id == self_node_id_)
                continue;
              entry.record = r;
              entry.last_updated_ms = nowMs();
            }
        }
      catch(const std::exception &e)
        {
          std::cerr << "[Gossip] Seed contact failed for " << seed << ": "
                    << e.what() << std::endl;
        }
    }

  gossip_thread_ = std::thread([this]() -> void { gossipLoop(); });
}

void GossipMembership::stop()
{
  running_ = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = member_table_.find(self_node_id_);
    if(it != member_table_.end())
      {
        it->second.record.status = NodeStatus::DEAD;
        it->second.record.heartbeat_counter++;
      }
  }
  // One final gossip round to announce departure
  doGossipRound();
  if(gossip_thread_.joinable())
    gossip_thread_.join();
}

void GossipMembership::setChangeCallback(MembershipChangeCallback cb)
{
  change_callback_ = std::move(cb);
}

std::vector<MemberRecord>
GossipMembership::mergeView(const std::vector<MemberRecord> &incoming)
{
  std::lock_guard<std::mutex> lock(mutex_);
  bool changed = false;

  for(const auto &r_in : incoming)
    {
      if(r_in.node_id == self_node_id_)
        continue; // never overwrite self from peer

      auto it = member_table_.find(r_in.node_id);
      if(it == member_table_.end())
        {
          // Unknown node – add it
          MemberEntry e;
          e.record = r_in;
          e.last_updated_ms = nowMs();
          member_table_[r_in.node_id] = e;
          changed = true;
          std::cout << "[Gossip] Discovered new node " << r_in.node_id
                    << " at " << r_in.endpoint << " ("
                    << static_cast<int>(r_in.status) << ")" << std::endl;
          continue;
        }

      MemberEntry &local = it->second;

      // Generation check – restarted node
      if(r_in.generation > local.record.generation)
        {
          local.record = r_in;
          local.last_updated_ms = nowMs();
          changed = true;
          continue;
        }

      if(r_in.generation < local.record.generation)
        continue; // stale generation, ignore

      // Same generation – heartbeat rules
      if(r_in.heartbeat_counter > local.record.heartbeat_counter)
        {
          local.record.heartbeat_counter = r_in.heartbeat_counter;
          local.record.status = r_in.status;
          local.last_updated_ms = nowMs();
          changed = true;
        }
      else if(r_in.heartbeat_counter == local.record.heartbeat_counter
              && static_cast<uint8_t>(r_in.status)
                   > static_cast<uint8_t>(local.record.status))
        {
          // Same heartbeat but more severe status propagates
          local.record.status = r_in.status;
          local.last_updated_ms = nowMs();
          changed = true;
        }
    }

  // Build response: return our merged view
  std::vector<MemberRecord> response;
  response.reserve(member_table_.size());
  for(const auto &[_, e] : member_table_)
    response.push_back(e.record);

  if(changed)
    notifyChange();

  return response;
}

std::vector<MemberRecord> GossipMembership::getMemberView() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<MemberRecord> view;
  view.reserve(member_table_.size());
  for(const auto &[_, e] : member_table_)
    view.push_back(e.record);
  return view;
}

// ---- private helpers ----

void GossipMembership::gossipLoop()
{
  while(running_)
    {
      std::this_thread::sleep_for(
        std::chrono::milliseconds(gossip_interval_ms_));
      if(!running_)
        break;

      {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = member_table_.find(self_node_id_);
        if(it != member_table_.end())
          {
            it->second.record.heartbeat_counter++;
            it->second.last_updated_ms = nowMs();
          }
        detectFailures();
      }

      doGossipRound();
    }
}

void GossipMembership::doGossipRound()
{
  std::string peer;
  std::vector<MemberRecord> local_view;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    peer = pickRandomPeer();
    if(peer.empty())
      return;
    local_view.reserve(member_table_.size());
    for(const auto &[_, e] : member_table_)
      local_view.push_back(e.record);
  }

  try
    {
      auto response = sendGossipExchange(peer, local_view);
      {
        std::lock_guard<std::mutex> lock(mutex_);
        bool changed = false;
        for(const auto &r : response)
          {
            if(r.node_id == self_node_id_)
              continue;
            auto it = member_table_.find(r.node_id);
            if(it == member_table_.end())
              {
                MemberEntry e;
                e.record = r;
                e.last_updated_ms = nowMs();
                member_table_[r.node_id] = e;
                changed = true;
                continue;
              }
            MemberEntry &local = it->second;
            if(r.generation > local.record.generation
               || (r.generation == local.record.generation
                   && r.heartbeat_counter > local.record.heartbeat_counter)
               || (r.generation == local.record.generation
                   && r.heartbeat_counter == local.record.heartbeat_counter
                   && static_cast<uint8_t>(r.status)
                        > static_cast<uint8_t>(local.record.status)))
              {
                local.record = r;
                local.last_updated_ms = nowMs();
                changed = true;
              }
          }
        if(changed)
          notifyChange();
      }
    }
  catch(const std::exception &e)
    {
      std::cerr << "[Gossip] Exchange with " << peer << " failed: " << e.what()
                << std::endl;
    }
}

std::vector<MemberRecord> GossipMembership::sendGossipExchange(
  const std::string &peer_endpoint,
  const std::vector<MemberRecord> &local_view)
{
  tl::endpoint ep = engine_.lookup(peer_endpoint);
  tl::provider_handle ph(ep, provider_id_);
  auto timeout = std::chrono::milliseconds(gossip_interval_ms_);
  return gossip_exchange_rpc_.on(ph)
    .timed(timeout, local_view)
    .as<std::vector<MemberRecord>>();
}

void GossipMembership::detectFailures()
{
  bool changed = false;
  const uint64_t now = nowMs();

  for(auto &[id, entry] : member_table_)
    {
      if(id == self_node_id_)
        continue;

      const uint64_t age = now - entry.last_updated_ms;

      if(entry.record.status == NodeStatus::ALIVE
         && age > static_cast<uint64_t>(suspect_threshold_ms_))
        {
          entry.record.status = NodeStatus::SUSPECT;
          std::cout << "[Gossip] Node " << id << " now SUSPECT (age=" << age
                    << "ms)" << std::endl;
          changed = true;
        }

      if(entry.record.status == NodeStatus::SUSPECT
         && age > static_cast<uint64_t>(dead_threshold_ms_))
        {
          entry.record.status = NodeStatus::DEAD;
          std::cout << "[Gossip] Node " << id << " now DEAD (age=" << age
                    << "ms)" << std::endl;
          changed = true;
        }
    }

  if(changed)
    notifyChange();
}

std::string GossipMembership::pickRandomPeer() const
{
  std::vector<std::string> alive_peers;
  for(const auto &[id, entry] : member_table_)
    {
      if(id != self_node_id_ && entry.record.status == NodeStatus::ALIVE)
        {
          alive_peers.push_back(entry.record.endpoint);
        }
    }
  if(alive_peers.empty())
    return {};
  return alive_peers[randomInt(static_cast<int>(alive_peers.size()))];
}

void GossipMembership::notifyChange()
{
  if(!change_callback_)
    return;

  std::unordered_map<int, std::string> live;
  for(const auto &[id, entry] : member_table_)
    {
      if(entry.record.status == NodeStatus::ALIVE)
        live[id] = entry.record.endpoint;
    }
  change_callback_(live);
}

uint32_t GossipMembership::loadGeneration()
{
  std::ifstream f(generationFilePath(self_node_id_));
  uint32_t gen = 0;
  if(f)
    f >> gen;
  return gen;
}

void GossipMembership::saveGeneration(uint32_t gen)
{
  std::ofstream f(generationFilePath(self_node_id_));
  f << gen;
}

uint64_t GossipMembership::currentTimeMs() { return nowMs(); }
