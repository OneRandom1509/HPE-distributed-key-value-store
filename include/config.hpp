#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

class Config {
public:
    Config(const std::string& filename);

    uint16_t read_provider_id() const;
    std::string read_protocol() const;
    int read_count() const;
    std::string get_endpoint(int node_id) const;
    size_t read_size() const;
    std::string read_ip() const;
private:
    nlohmann::json config_json;
};

