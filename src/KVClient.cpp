#include "KVClient.hpp"
#include "KVStore.hpp"
#include "Timer.hpp"
#include <thallium/serialization/stl/string.hpp>
KVClient::KVClient(const std::string &protocol, uint16_t provider_id)
    : myEngine(protocol, THALLIUM_CLIENT_MODE), provider_id(provider_id)
{
  std::cout << "[DEBUG] Thallium initialized with protocol: " << protocol
            << std::endl;
}
std::string KVClient::fetch(int key, std::string &server_endpoint)
{
  std::string value = "";
  try
    {
      std::cout << "Key not found locally. Fetching from server.\n";
      tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
      tl::endpoint server_ep = myEngine.lookup(server_endpoint);
      tl::provider_handle ph(server_ep, provider_id);
      Timer t;
      t.start();
      value = remote_kv_fetch.on(ph)(key).as<std::string>();
      t.stop();
      std::cout << "Key Found on the Server.\n";
      std::cout << key << "->" << value << '\n';
      t.print("Fetch");
    }
  catch(const std::exception &e)
    {
      std::cerr << "Fetch operation failed: " << e.what() << std::endl;
      return e.what();
    }
  return value;
}
void KVClient::insert(int key, const std::string value,
                      const std::string &server_endpoint)
{
  try
    {
      tl::remote_procedure remote_kv_insert = myEngine.define("kv_insert");
      tl::endpoint server_ep = myEngine.lookup(server_endpoint);
      tl::provider_handle ph(server_ep, provider_id);
      Timer t;
      t.start();
      remote_kv_insert.on(ph)(key, value);
      t.stop();
      std::cout << "Inserted on the server successfully: " << key << " -> "
                << value << std::endl;
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
      tl::endpoint server_ep = myEngine.lookup(server_endpoint);
      tl::provider_handle ph(server_ep, provider_id);
      Timer t;
      t.start();
      remote_kv_update.on(ph)(key, value);
      t.stop();
      std::cout << "Updated successfully: " << key << " -> " << value
                << std::endl;
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
      tl::endpoint server_ep = myEngine.lookup(server_endpoint);
      tl::provider_handle ph(server_ep, provider_id);
      Timer t;
      t.start();
      remote_kv_delete.on(ph)(key);
      t.stop();
      std::cout << "Deleted successfully: " << key << std::endl;
      t.print("Delete");
    }
  catch(const std::exception &e)
    {
      std::cerr << "Delete operation failed: " << e.what() << std::endl;
    }
}
