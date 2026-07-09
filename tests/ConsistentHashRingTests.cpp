#include "ConsistentHashRing.hpp"

#include <iostream>
#include <unordered_map>
#include <vector>

namespace
{
  bool check(bool condition, const char *message)
  {
    if(!condition)
      {
        std::cerr << "FAIL: " << message << '\n';
        return false;
      }
    return true;
  }

    void pass(const char *message)
    {
      std::cout << "PASS: " << message << '\n';
    }

  bool checkEqual(int actual, int expected, const char *message)
  {
    if(actual != expected)
      {
        std::cerr << "FAIL: " << message << " (expected " << expected
                  << ", got " << actual << ")\n";
        return false;
      }
    return true;
  }
} // namespace

int main()
{
  bool ok = true;

  {
    bool case_ok = true;
    std::unordered_map<int, std::string> nodes;
    ConsistentHashRing ring(nodes, 4);
    case_ok &= checkEqual(ring.getPrimaryNode(42), -1,
                        "empty ring should return -1 for primary");
    case_ok &= checkEqual(ring.getBuddyNode(42), -1,
                        "empty ring should return -1 for buddy");
    ok &= case_ok;
    if(case_ok)
      pass("empty ring returns -1 for primary and buddy");
  }

  {
    bool case_ok = true;
    std::unordered_map<int, std::string> nodes{{0, "127.0.0.1:5000"}};
    ConsistentHashRing ring(nodes, 4);
    for(int key : {-1000, -1, 0, 1, 42, 999999})
      {
        case_ok &= checkEqual(ring.getPrimaryNode(key), 0,
                         "single-node ring primary should stay on node 0");
        case_ok &= checkEqual(ring.getBuddyNode(key), 0,
                         "single-node ring buddy should stay on node 0");
      }
    ok &= case_ok;
    if(case_ok)
      pass("single-node ring stays on node 0");
  }

  {
    bool case_ok = true;
    std::unordered_map<int, std::string> nodes{{0, "127.0.0.1:5000"},
                                               {1, "127.0.0.1:5001"}};
    ConsistentHashRing ring(nodes, 8);
    bool saw_zero = false;
    bool saw_one = false;

    for(int key = 0; key < 200; ++key)
      {
        const int primary = ring.getPrimaryNode(key);
        const int buddy = ring.getBuddyNode(key);
        case_ok &= check(primary == 0 || primary == 1,
                    "primary should resolve to one of the configured nodes");
        case_ok &= check(buddy == 0 || buddy == 1,
                    "buddy should resolve to one of the configured nodes");
        case_ok &= check(primary != buddy,
                    "buddy should differ from primary when two nodes exist");
        saw_zero |= primary == 0;
        saw_one |= primary == 1;
      }

    case_ok &= check(saw_zero && saw_one,
                     "two-node ring should map some keys to each node");
    ok &= case_ok;
    if(case_ok)
      pass("two-node ring maps keys to both nodes and picks a different buddy");
  }

  {
    bool case_ok = true;
    std::unordered_map<int, std::string> one_node{{7, "127.0.0.1:5007"}};
    ConsistentHashRing ring(one_node, 3);
    case_ok &= checkEqual(ring.getPrimaryNode(10), 7,
                          "initial one-node ring should map to node 7");

    std::unordered_map<int, std::string> two_nodes{{7, "127.0.0.1:5007"},
                                                   {8, "127.0.0.1:5008"}};
    ring.rebuild(two_nodes, 3);

    bool saw_seven = false;
    bool saw_eight = false;
    for(int key = 0; key < 100; ++key)
      {
        const int primary = ring.getPrimaryNode(key);
        saw_seven |= primary == 7;
        saw_eight |= primary == 8;
      }

    case_ok &= check(saw_seven && saw_eight,
                     "rebuild should replace the ring contents");
    ok &= case_ok;
    if(case_ok)
      pass("rebuild replaces ring contents and keeps both nodes reachable");
  }

  return ok ? 0 : 1;
}
