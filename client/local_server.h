#pragma once

#include "tunnel.h"

#include <boost/noncopyable.hpp>
#include <muduo/net/TcpServer.h>
#include <unordered_map>

namespace zy
{
class local_server : boost::noncopyable
{
 public:
  enum conState
  {
    kStart,
    kVerified,
    kGotcmd,
    kTransport
  };

  local_server(muduo::net::EventLoop* loop, const muduo::net::InetAddress& local_addr, const muduo::net::InetAddress& remote_addr,
               const std::string& passwd);

  void onConnection(const muduo::net::TcpConnectionPtr& con);

  void onMessage(const muduo::net::TcpConnectionPtr& con, muduo::net::Buffer* buf, muduo::Timestamp receiveTime);

  void set_con_state(const muduo::string& con_name, conState state);

  void start() { server_.start(); }

  void  set_timeout(double timeout) { timeout_ = timeout; }

 private:

  void erase_from_tunnel(const muduo::string& con_name);

  struct TunnelState
  {
    TunnelState() = default;

    TunnelState(conState state_)
        : state(state_),
          tunnel()
    { }

    conState state;
    TunnelPtr tunnel;
  };

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  muduo::net::InetAddress remote_addr_;
  std::string passwd_;
  std::unordered_map<muduo::string, TunnelState> tunnels_;
  double timeout_;
};
}