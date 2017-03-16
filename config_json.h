#pragma once

#include <boost/noncopyable.hpp>
#include <stdio.h>
#include <string>
#include <rapidjson/document.h>

namespace zy
{
namespace util {
class file_reader: boost::noncopyable {
 public:
  //die if error
  file_reader(const std::string &path);

  ~file_reader();

  FILE *fp() { return fp_; }
 private:
  FILE *fp_;
};
}

class config_json : boost::noncopyable
{
 public:
  // die if error
  explicit config_json(const std::string& path, bool server = true);

  ~config_json() = default;

  std::string server_addr() const;
  std::string password() const;
  std::string local_addr() const;

  int16_t server_port() const;
  int16_t local_port() const;

  int timeout() const;

  int dns_timeout() const;
  
  bool server_ipv6() const;

 private:
  rapidjson::Document config_;
};
}
