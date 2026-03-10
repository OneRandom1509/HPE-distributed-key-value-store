#ifndef KVSERVER_HPP
#define KVSERVER_HPP

#include <iostream>
#include <memory>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <unordered_map>
#include "KVStore.hpp"

namespace tl = thallium;

class KVServer : public tl::provider<KVServer> {
private:
    KvStore& kv;
    void kv_fetch(const tl::request& req, int key);
    void kv_insert(const tl::request& req, int key, std::string value);
    void kv_update(const tl::request& req, int key, std::string value);
    void kv_delete(const tl::request& req, int key);  // Add delete method

public:
    KVServer(tl::engine &e, KvStore& kv_ref, uint16_t provider_id);
};

#endif // KVSERVER_HPP
