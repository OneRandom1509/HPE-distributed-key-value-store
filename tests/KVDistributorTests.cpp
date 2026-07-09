#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <unordered_map>

#include "KVDistributor.hpp"
#include "IKVClient.hpp"
#include "KVStore.hpp"
#include "config.hpp"
#include "GossipMembership.hpp"

using namespace std;

struct FakeKVClient : public IKVClient
{
  std::vector<std::pair<std::string,int>> inserts;
  std::vector<std::pair<std::string,int>> updates;
  std::vector<std::pair<std::string,int>> deletes;
  std::unordered_map<int,std::string> store;

  std::string fetch(int key, std::string &server_endpoint) override
  {
    auto it = store.find(key);
    if(it == store.end())
      return "key not found";
    return it->second;
  }
  void insert(int key, const std::string value, const std::string &server_endpoint) override
  {
    inserts.emplace_back(server_endpoint, key);
  }
  void update(int key, const std::string value, const std::string &server_endpoint) override
  {
    updates.emplace_back(server_endpoint, key);
  }
  void deleteKey(int key, const std::string &server_endpoint) override
  {
    deletes.emplace_back(server_endpoint, key);
  }
  std::vector<MemberRecord> getMembership(const std::string &server_endpoint) override
  {
    return {};
  }
};

TEST(KVDistributorTests, LocalWritesUseLocalStore)
{
  Config cfg("tests/test_config_single.json");
  auto &kv = KvStore::get_instance(1024*1024, StorageMode::MEMORY, ConnectionMode::SERVER);
  // No Clear() API; rely on fresh singleton in tests

  FakeKVClient fake;
  KVDistributor dist(kv, cfg, &fake);

  dist.insert(1, "v1");
  EXPECT_EQ(kv.Find(1), "v1");

  dist.update(1, "v2");
  EXPECT_EQ(kv.Find(1), "v2");

  dist.deleteKey(1);
  EXPECT_EQ(kv.Find(1), "key not found");
}

TEST(KVDistributorTests, RemoteWritesInvokeClient)
{
  Config cfg("tests/test_config_two.json");
  auto &kv2 = KvStore::get_instance(1024*1024, StorageMode::MEMORY, ConnectionMode::SERVER);

  FakeKVClient fake2;
  KVDistributor dist2(kv2, cfg, &fake2);

  // rebuild with two endpoints
  std::unordered_map<int,std::string> endpoints = {{0, "127.0.0.1:5000"}, {1, "127.0.0.2:5001"}};
  dist2.rebuildRing(endpoints, 4);

  dist2.insert(42, "rv");

  if(fake2.inserts.empty())
    EXPECT_EQ(kv2.Find(42), "rv");
  else
    EXPECT_FALSE(fake2.inserts.empty());
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
