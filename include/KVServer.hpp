#ifndef KVSERVER_HPP
#define KVSERVER_HPP

#include <iostream>
#include <memory>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <unordered_map>
#include <vector>
#include "KVStore.hpp"
#include "GossipMembership.hpp"

namespace tl = thallium;

class KVServer : public tl::provider<KVServer>
{
private:
  KvStore &kv;
  GossipMembership *membership_;

  void kv_fetch(const tl::request &req, int key);
  void kv_insert(const tl::request &req, int key, std::string value);
  void kv_update(const tl::request &req, int key, std::string value);
  void kv_delete(const tl::request &req, int key);
  void
  gossip_exchange(const tl::request &req, std::vector<MemberRecord> incoming);
  void get_membership(const tl::request &req);

public:
  KVServer(tl::engine &e, KvStore &kv_ref, uint16_t provider_id,
           GossipMembership *membership = nullptr);
};

#endif // KVSERVER_HPP
