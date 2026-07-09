#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

struct GossipConfig
{
  int interval_ms;
  int suspect_threshold_ms;
  int dead_threshold_ms;
  std::vector<std::string> seed_nodes;
};

class Config
{
public:
  Config(const std::string &filename);

  uint16_t read_provider_id() const;
  std::string read_protocol() const;
  int read_count() const;
  std::string get_endpoint(int node_id) const;
  size_t read_size() const;
  std::string read_ip() const;
  GossipConfig read_gossip_config() const;

private:
  nlohmann::json config_json;
};
