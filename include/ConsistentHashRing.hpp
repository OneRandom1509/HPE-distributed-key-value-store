#ifndef CONSISTENT_HASH_RING_HPP
#define CONSISTENT_HASH_RING_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

struct KeyRange
{
  uint32_t start; // inclusive
  uint32_t end;   // exclusive; if start >= end the range wraps past UINT32_MAX
};

struct KeyRangeAssignment
{
  KeyRange range;
  int primary_node_id;
  int buddy_node_id;
};

class ConsistentHashRing
{
public:
  ConsistentHashRing() = default;
  ConsistentHashRing(const std::unordered_map<int, std::string> &node_endpoints,
                     int virtual_nodes_per_node = 2);

  int getPrimaryNode(int key) const;
  int getBuddyNode(int key) const;
  std::unordered_map<int, std::vector<KeyRange>> getKeyRanges() const;
  std::vector<KeyRangeAssignment> getKeyRangeAssignments() const;
  void rebuild(const std::unordered_map<int, std::string> &node_endpoints,
               int virtual_nodes_per_node = 2);

private:
  struct Token
  {
    uint32_t position;
    int node_id;
  };
  std::vector<Token> ring_;
  int virtual_nodes_per_node_;

  static uint32_t hashNodeId(int node_id);
  static uint32_t hashKey(int key);
  static uint32_t hashToken(const std::string &token_str);
  std::vector<Token>::const_iterator findToken(uint32_t position) const;
};

#endif // CONSISTENT_HASH_RING_HPP
