#include "ConsistentHashRing.hpp"
#include "MurmurHash3.hpp"
#include <algorithm>
#include <string>

using namespace std;

static inline uint32_t to_u32(const string &s)
{
  return murmur3_32(s.data(), s.size(), 0);
}

uint32_t ConsistentHashRing::hashNodeId(int node_id)
{
  std::string s = std::to_string(node_id);
  return murmur3_32(s.data(), static_cast<int>(s.size()), 0);
}

uint32_t ConsistentHashRing::hashKey(int key)
{
  std::string s = std::to_string(key);
  return murmur3_32(s.data(), static_cast<int>(s.size()), 0);
}

uint32_t ConsistentHashRing::hashToken(const std::string &token_str)
{
  return to_u32(token_str);
}

ConsistentHashRing::ConsistentHashRing(
  const unordered_map<int, string> &node_endpoints, int virtual_nodes_per_node)
    : virtual_nodes_per_node_(virtual_nodes_per_node)
{
  rebuild(node_endpoints, virtual_nodes_per_node);
}

void ConsistentHashRing::rebuild(
  const unordered_map<int, string> &node_endpoints, int virtual_nodes_per_node)
{
  virtual_nodes_per_node_ = virtual_nodes_per_node;
  ring_.clear();

  if(virtual_nodes_per_node_ <= 0)
    return;

  const uint64_t ring_space = 1ULL << 32;
  const uint64_t interval
    = max<uint64_t>(1, ring_space / virtual_nodes_per_node_);

  for(const auto &p : node_endpoints)
    {
      int node_id = p.first;
      const uint32_t base_position = hashNodeId(node_id);
      for(int v = 0; v < virtual_nodes_per_node_; ++v)
        {
          Token t;
          t.position = static_cast<uint32_t>(
            base_position + (static_cast<uint64_t>(v) * interval));
          t.node_id = node_id;
          ring_.push_back(t);
        }
    }
  stable_sort(ring_.begin(), ring_.end(),
              [](const Token &a, const Token &b) -> bool {
                if(a.position != b.position)
                  return a.position < b.position;
                return a.node_id < b.node_id;
              });
}
std::vector<ConsistentHashRing::Token>::const_iterator
ConsistentHashRing::findToken(uint32_t position) const
{
  auto it = std::lower_bound(
    ring_.begin(), ring_.end(), position,
    [](const Token &t, uint32_t v) { return t.position < v; });
  if(it == ring_.end())
    return ring_.begin();
  return it;
}

int ConsistentHashRing::getPrimaryNode(int key) const
{
  if(ring_.empty())
    return -1;
  uint32_t pos = hashKey(key);
  auto it = findToken(pos);
  return it->node_id;
}

int ConsistentHashRing::getBuddyNode(int key) const
{
  if(ring_.empty())
    return -1;
  uint32_t pos = hashKey(key);
  auto it = findToken(pos);
  auto start = it;
  // advance to find first token with different node_id
  do
    {
      ++it;
      if(it == ring_.end())
        it = ring_.begin();
      if(it->node_id != start->node_id)
        return it->node_id;
  } while(it != start);
  // single-node cluster
  return start->node_id;
}

std::vector<KeyRangeAssignment>
ConsistentHashRing::getKeyRangeAssignments() const
{
  std::vector<KeyRangeAssignment> result;

  if(ring_.empty())
    return result;

  const size_t n = ring_.size();
  for(size_t i = 0; i < n; ++i)
    {
      uint32_t cur_pos = ring_[i].position;
      uint32_t prev_pos = ring_[(i + n - 1) % n].position;
      int primary_id = ring_[i].node_id;

      uint32_t start = prev_pos + 1;
      uint32_t end = cur_pos + 1;

      int buddy_id = primary_id;
      for(size_t j = 1; j < n; ++j)
        {
          size_t idx = (i + j) % n;
          if(ring_[idx].node_id != primary_id)
            {
              buddy_id = ring_[idx].node_id;
              break;
            }
        }

      if(start < end)
        {
          result.push_back(
            {{.start = start, .end = end}, primary_id, buddy_id});
        }
      else if(start > end)
        {
          result.push_back(
            {{.start = start, .end = UINT32_MAX}, primary_id, buddy_id});
          if(end > 0)
            result.push_back({{.start = 0, .end = end}, primary_id, buddy_id});
        }
      else
        {
          if(n == 1)
            result.push_back(
              {{.start = 0, .end = UINT32_MAX}, primary_id, buddy_id});
        }
    }

  return result;
}

std::unordered_map<int, std::vector<KeyRange>>
ConsistentHashRing::getKeyRanges() const
{
  std::unordered_map<int, std::vector<KeyRange>> result;

  if(ring_.empty())
    return result;

  const size_t n = ring_.size();
  for(size_t i = 0; i < n; ++i)
    {
      // This token owns keys whose hash is in (prev_pos, cur_pos].
      // Represent as half-open ranges [start, end) where
      // start = prev_pos + 1 and end = cur_pos + 1.
      uint32_t cur_pos = ring_[i].position;
      uint32_t prev_pos = ring_[(i + n - 1) % n].position;
      int node_id = ring_[i].node_id;

      uint32_t start = prev_pos + 1;
      uint32_t end = cur_pos + 1;

      if(start < end)
        {
          result[node_id].push_back({start, end});
        }
      else if(start > end)
        {
          result[node_id].push_back({start, UINT32_MAX});
          if(end > 0)
            result[node_id].push_back({0, end});
        }
      else
        {
          if(n == 1)
            result[node_id].push_back({0, UINT32_MAX});
        }
    }

  return result;
}
