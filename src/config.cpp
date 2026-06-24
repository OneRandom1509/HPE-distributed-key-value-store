#include "config.hpp"
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

Config::Config(const std::string &filename)
{
  std::ifstream file(filename);
  if(!file.is_open())
    {
      throw std::runtime_error("Could not open config file: " + filename);
    }
  file >> config_json;
}

uint16_t Config::read_provider_id() const
{
  return config_json.at("provider_id").get<uint16_t>();
}

std::string Config::read_protocol() const
{
  return config_json.at("protocol").get<std::string>();
}

size_t Config::read_size() const
{
  int size_in_mb = config_json.at("size").get<int>();
  return static_cast<size_t>(size_in_mb) * 1024 * 1024; // Convert MB to bytes
}

std::string Config::read_ip() const
{
  return config_json.value("local_ip", "");
}

GossipConfig Config::read_gossip_config() const
{
  GossipConfig gc;
  if(!config_json.contains("gossip"))
    {
      gc.interval_ms = 1000;
      gc.suspect_threshold_ms = 3000;
      gc.dead_threshold_ms = 10000;
      return gc;
    }
  const auto &g = config_json.at("gossip");
  gc.interval_ms = g.value("interval_ms", 1000);
  gc.suspect_threshold_ms = g.value("suspect_threshold_ms", 3000);
  gc.dead_threshold_ms = g.value("dead_threshold_ms", 10000);
  if(g.contains("seed_nodes"))
    {
      for(const auto &s : g.at("seed_nodes"))
        gc.seed_nodes.push_back(s.get<std::string>());
    }
  return gc;
}

std::vector<std::string> Config::read_seed_nodes() const
{
  if(config_json.contains("seed_nodes"))
    return config_json["seed_nodes"].get<std::vector<std::string>>();
  if(config_json.contains("gossip")
     && config_json["gossip"].contains("seed_nodes"))
    {
      std::vector<std::string> nodes;
      for(const auto &s : config_json["gossip"]["seed_nodes"])
        nodes.push_back(s.get<std::string>());
      return nodes;
    }
  return {};
}
