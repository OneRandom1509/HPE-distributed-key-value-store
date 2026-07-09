#ifndef IKVCLIENT_HPP
#define IKVCLIENT_HPP

#include <string>
#include <vector>

struct MemberRecord;

class IKVClient
{
public:
  virtual ~IKVClient() = default;
  virtual std::string fetch(int key, std::string &server_endpoint) = 0;
  virtual void
  insert(int key, const std::string value, const std::string &server_endpoint)
    = 0;
  virtual void
  update(int key, const std::string value, const std::string &server_endpoint)
    = 0;
  virtual void deleteKey(int key, const std::string &server_endpoint) = 0;
  virtual std::vector<MemberRecord>
  getMembership(const std::string &server_endpoint) = 0;
};

#endif // IKVCLIENT_HPP
