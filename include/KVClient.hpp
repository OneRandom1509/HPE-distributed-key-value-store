#ifndef KVCLIENT_HPP
#define KVCLIENT_HPP

#include <iostream>
#include <thallium.hpp>
#include <unordered_map>
#include "KVStore.hpp"
#include "IKVClient.hpp"
#include <chrono>

namespace tl = thallium;

class KVClient : public IKVClient
{
private:
  tl::engine myEngine;
  uint16_t provider_id;
  std::string protocol;
  uint64_t rpc_timeout_ms = 0; // 0 means no timeout (blocking)

public:
  KVClient(const std::string &protocol, uint16_t provider_id,
           uint64_t rpc_timeout_ms = 0);
  std::string fetch(int key, std::string &server_endpoint) override;
  void insert(int key, const std::string value,
              const std::string &server_endpoint) override;
  void update(int key, const std::string value,
              const std::string &server_endpoint) override;
  void deleteKey(int key, const std::string &server_endpoint) override;
};

#endif // KVCLIENT_HPP
