#include "config_json.h"

#include <muduo/base/Logging.h>
#include <rapidjson/filereadstream.h>

using namespace zy;
using namespace zy::util;
using namespace rapidjson;

file_reader::file_reader(const std::string &path)
    : fp_(nullptr)
{
  if((fp_ = fopen(path.c_str(), "rb")) == nullptr)
  {
    LOG_FATAL << "fail to open config file with given path : " << path << " the reason is " << strerror(errno);
  }
}

file_reader::~file_reader()
{
  if(fp_)
    ::fclose(fp_);
}

config_json::config_json(const std::string &path, bool server)
{
  file_reader reader(path);
  char buffer[1024];
  FileReadStream is(reader.fp(), buffer, sizeof(buffer));
  config_.ParseStream(is);

  if(!config_.HasMember("server_port") || !config_["server_port"].IsNumber())
  {
    LOG_FATAL << "config without server_port";
  }
  if(!config_.HasMember("password") || !config_["password"].IsString())
  {
    LOG_FATAL << "config without password";
  }
  if(!config_.HasMember("timeout") || !config_["timeout"].IsNumber())
  {
    LOG_FATAL << "config without timeout";
  }
  if(!config_.HasMember("server_ipv6") || !config_["server_ipv6"].IsBool())
  {
    LOG_FATAL << "config without server_ipv6";
  }
  if(server)
  {
    if(!config_.HasMember("dns_timeout") || !config_["dns_timeout"].IsNumber())
    {
      LOG_FATAL << "config without dns_timeout";
    }
  }
  else
  {
    if(!config_.HasMember("server") || !config_["server"].IsString())
    {
      LOG_FATAL << "config with out server address";
    }
    if(!config_.HasMember("local_address") || !config_["local_address"].IsString())
    {
      LOG_FATAL << "config without local_address";
    }
    if(!config_.HasMember("local_port") || !config_["local_port"].IsNumber())
    {
      LOG_FATAL << "config without local_port";
    }

  }
}

std::string config_json::server_addr() const
{
  return config_["server"].GetString();
}

std::string config_json::password() const {
  return config_["password"].GetString();
}

std::string config_json::local_addr() const {
  return config_["local_address"].GetString();
}

int16_t config_json::server_port() const {
  return static_cast<int16_t>(config_["server_port"].GetInt());
}

int16_t config_json::local_port() const {
  return static_cast<int16_t>(config_["local_port"].GetInt());
}

int config_json::timeout() const {
  return config_["timeout"].GetInt();
}

int config_json::dns_timeout() const {
  return config_["dns_timeout"].GetInt();
}

bool config_json::server_ipv6() const {
  return config_["server_ipv6"].GetBool();
}
