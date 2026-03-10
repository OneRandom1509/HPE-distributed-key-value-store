#include "config.hpp"
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

Config::Config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + filename);
    }
    file >> config_json;
}

uint16_t Config::read_provider_id() const {
    return config_json.at("provider_id").get<uint16_t>();
}

std::string Config::read_protocol() const {
    return config_json.at("protocol").get<std::string>();
}

int Config::read_count() const {
    return config_json.at("count_of_node").get<int>();
}

std::string Config::get_endpoint(int node_id) const {
    const auto& ip_map = config_json.at("ip_addresses");
    return ip_map.at(std::to_string(node_id)).get<std::string>();
}

size_t Config::read_size() const {
    int size_in_mb = config_json.at("size").get<int>();
    return static_cast<size_t>(size_in_mb) * 1024 * 1024;  // Convert MB to bytes
}

std::string Config::read_ip() const{
    return config_json["local_ip"];  // assuming JSON has key "local_ip"
}
