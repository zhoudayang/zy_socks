#pragma once

#include "Resolver.h"
#include "tunnel.h"

#include <boost/noncopyable.hpp>
#include <muduo/net/TcpServer.h>
#include <unordered_map>

namespace zy
{
class socks_server : boost::noncopyable
{
 public:
  enum conState
  {
    kStart, // just connected
    kGotcmd, // get command already
    kResolved, // got ip address already
    kTransport // connect to remote server successful, now swap data
  };
  socks_server(muduo::net::EventLoop* loop, const muduo::net::InetAddress& addr,
               const std::string& passwd);

  void onConnection(const muduo::net::TcpConnectionPtr& con);

  void onMessage(const muduo::net::TcpConnectionPtr& con, muduo::net::Buffer* buf, muduo::Timestamp receiveTime);

 private:

  void erase_from_con_states(const muduo::string& con_name);

  void erase_from_tunnels(const muduo::string& con_name);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  Resolver resolver_;
  std::string passwd_;
  std::unordered_map<muduo::string, conState> con_states_;
  std::unordered_map<muduo::string, TunnelPtr> tunnels_;
};

}