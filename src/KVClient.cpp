#include "KVClient.hpp"
#include "Timer.hpp"
#include <thallium/serialization/stl/string.hpp>
#include <thallium/serialization/stl/vector.hpp>
#include <chrono>

KVClient::KVClient(const std::string &protocol, uint16_t provider_id,
                   uint64_t rpc_timeout_ms)
    : myEngine(protocol, THALLIUM_CLIENT_MODE), provider_id(provider_id),
      protocol(protocol), rpc_timeout_ms(rpc_timeout_ms)
{
  std::cout << "[DEBUG] Thallium initialized with protocol: " << protocol;
  if(rpc_timeout_ms > 0)
    std::cout << " timeout=" << rpc_timeout_ms << "ms";
  std::cout << std::endl;
}

std::string KVClient::fetch(int key, std::string &server_endpoint)
{
  std::string value = "";
  try
    {
      std::cout << "Key not found locally. Fetching from server.\n";
      tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
      std::string full_ep = protocol + "://" + server_endpoint;
      tl::endpoint server_ep = myEngine.lookup(full_ep);
      tl::provider_handle ph(server_ep, provider_id);

      Timer t;
      t.start();
      if(rpc_timeout_ms > 0)
        {
          auto req = remote_kv_fetch.on(ph).timed_async(
            std::chrono::milliseconds(rpc_timeout_ms), key);
          value = req.wait().as<std::string>();
        }
      else
        {
          value = remote_kv_fetch.on(ph)(key).as<std::string>();
        }
      t.stop();
      std::cout << "Key Found on the Server.\n";
      std::cout << "0x" << std::hex << key << std::dec << "->" << value
                << '\n';
      t.print("Fetch");
    }
  catch(const std::exception &e)
    {
      std::cerr << "Fetch operation failed: " << e.what() << std::endl;
      throw;
    }
  return value;
}

void KVClient::insert(int key, const std::string value,
                      const std::string &server_endpoint)
{
  try
    {
      tl::remote_procedure remote_kv_insert = myEngine.define("kv_insert");
      std::string full_ep = protocol + "://" + server_endpoint;
      tl::endpoint server_ep = myEngine.lookup(full_ep);
      tl::provider_handle ph(server_ep, provider_id);
      Timer t;
      t.start();
      if(rpc_timeout_ms > 0)
        {
          auto req = remote_kv_insert.on(ph).timed_async(
            std::chrono::milliseconds(rpc_timeout_ms), key, value);
          req.wait();
        }
      else
        {
          remote_kv_insert.on(ph)(key, value);
        }
      t.stop();
      std::cout << "Inserted on the server successfully: 0x" << std::hex << key
                << std::dec << " -> " << value << std::endl;
      t.print("Insert");
    }
  catch(const std::exception &e)
    {
      std::cerr << "Insert operation failed: " << e.what() << std::endl;
    }
}

void KVClient::update(int key, const std::string value,
                      const std::string &server_endpoint)
{
  try
    {
      tl::remote_procedure remote_kv_update = myEngine.define("kv_update");
      std::string full_ep = protocol + "://" + server_endpoint;
      tl::endpoint server_ep = myEngine.lookup(full_ep);
      tl::provider_handle ph(server_ep, provider_id);
      Timer t;
      t.start();
      if(rpc_timeout_ms > 0)
        {
          auto req = remote_kv_update.on(ph).timed_async(
            std::chrono::milliseconds(rpc_timeout_ms), key, value);
          req.wait();
        }
      else
        {
          remote_kv_update.on(ph)(key, value);
        }
      t.stop();
      std::cout << "Updated successfully: 0x" << std::hex << key << std::dec
                << " -> " << value << std::endl;
      t.print("Update");
    }
  catch(const std::exception &e)
    {
      std::cerr << "Update operation failed: " << e.what() << std::endl;
    }
}

void KVClient::deleteKey(int key, const std::string &server_endpoint)
{
  try
    {
      tl::remote_procedure remote_kv_delete = myEngine.define("kv_delete");
      std::string full_ep = protocol + "://" + server_endpoint;
      tl::endpoint server_ep = myEngine.lookup(full_ep);
      tl::provider_handle ph(server_ep, provider_id);
      Timer t;
      t.start();
      if(rpc_timeout_ms > 0)
        {
          auto req = remote_kv_delete.on(ph).timed_async(
            std::chrono::milliseconds(rpc_timeout_ms), key);
          req.wait();
        }
      else
        {
          remote_kv_delete.on(ph)(key);
        }
      t.stop();
      std::cout << "Deleted successfully: 0x" << std::hex << key << std::dec
                << std::endl;
      t.print("Delete");
    }
  catch(const std::exception &e)
    {
      std::cerr << "Delete operation failed: " << e.what() << std::endl;
    }
}

std::vector<MemberRecord>
KVClient::getMembership(const std::string &server_endpoint)
{
  try
    {
      tl::remote_procedure remote_get_membership
        = myEngine.define("get_membership");
      std::string full_ep = protocol + "://" + server_endpoint;
      tl::endpoint server_ep = myEngine.lookup(full_ep);
      tl::provider_handle ph(server_ep, provider_id);
      std::vector<MemberRecord> result;
      if(rpc_timeout_ms > 0)
        {
          auto req = remote_get_membership.on(ph).timed_async(
            std::chrono::milliseconds(rpc_timeout_ms));
          result = req.wait().as<std::vector<MemberRecord>>();
        }
      else
        {
          result
            = remote_get_membership.on(ph)().as<std::vector<MemberRecord>>();
        }
      return result;
    }
  catch(const std::exception &e)
    {
      std::cerr << "getMembership failed: " << e.what() << std::endl;
      return {};
    }
}
