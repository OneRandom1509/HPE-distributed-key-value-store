#include "KVServer.hpp"
#include "KVStore.hpp"
#include <stdexcept>
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include "Timer.hpp"

KVServer::KVServer(tl::engine &e, KvStore &kv_ref, uint16_t provider_id,
                   GossipMembership *membership)
    : tl::provider<KVServer>(e, provider_id), kv(kv_ref),
      membership_(membership)
{
  define("kv_fetch", &KVServer::kv_fetch);
  define("kv_insert", &KVServer::kv_insert);
  define("kv_update", &KVServer::kv_update);
  define("kv_delete", &KVServer::kv_delete);
  define("gossip_exchange", &KVServer::gossip_exchange);
  define("get_membership", &KVServer::get_membership);
}
void KVServer::kv_fetch(const tl::request &req, int key)
{
  Timer t;
  t.start();
  std::cout << "[Fetch] key=" << key << std::endl;
  try
    {
      std::string value = kv.Find(key);
      t.stop();
      std::cout << "[Fetch] Key: " << key << ", Value: " << value << std::endl;
      t.print("Fetch");
      req.respond(value);
    }
  catch(const std::exception &e)
    {
      std::cerr << "[Fetch Error] " << e.what() << std::endl;
      req.respond();
    }
}
void KVServer::kv_insert(const tl::request &req, int key, std::string value)
{
  Timer t;
  t.start();
  std::cout << "[Insert] " << key << " -> " << value << std::endl;
  try
    {
      kv.Insert(key, value);
      t.stop();
      t.print("Insert");
      req.respond(1);
    }
  catch(const std::exception &e)
    {
      std::cerr << "[Insert Error] " << e.what() << std::endl;
      req.respond(0);
    }
}
void KVServer::kv_update(const tl::request &req, int key, std::string value)
{
  Timer t;
  t.start();
  std::cout << "[Update] " << key << " -> " << value << std::endl;
  try
    {
      kv.Update(key, value);
      t.stop();
      t.print("Update");
      req.respond(1);
    }
  catch(const std::exception &e)
    {
      std::cerr << "[Update Error] " << e.what() << std::endl;
      req.respond(0);
    }
}
void KVServer::kv_delete(const tl::request &req, int key)
{
  Timer t;
  t.start();
  std::cout << "[Delete] key=" << key << std::endl;
  try
    {
      kv.Delete(key);
      t.stop();
      t.print("Delete");
      req.respond(1);
    }
  catch(const std::exception &e)
    {
      std::cerr << "[Delete Error] " << e.what() << std::endl;
      req.respond(0);
    }
}

void KVServer::gossip_exchange(const tl::request &req,
                               std::vector<MemberRecord> incoming)
{
  if(!membership_)
    {
      req.respond(std::vector<MemberRecord>{});
      return;
    }
  auto response = membership_->mergeView(std::move(incoming));
  req.respond(std::move(response));
}

void KVServer::get_membership(const tl::request &req)
{
  if(!membership_)
    {
      req.respond(std::vector<MemberRecord>{});
      return;
    }
  auto view = membership_->getMemberView();
  req.respond(std::move(view));
}
