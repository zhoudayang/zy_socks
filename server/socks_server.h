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
  
  void set_con_state(const muduo::string& name, conState state);
 
  void start() { server_.start(); }
  
  void set_dns_timeout(double timeout) { dns_timeout_ = timeout; }
  
  void set_tunnel_timeout(double timeout) { tunnel_timeout_ = timeout; }
  
 private:
    
  void onResolve(const muduo::net::TcpConnectionPtr& con, const muduo::net::InetAddress& addr);
  
  void onResolveError(const muduo::net::TcpConnectionPtr& con, const muduo::string& host);
  
  void erase_from_con_states(const muduo::string& con_name);

  void erase_from_tunnels(const muduo::string& con_name);

  void send_response_and_down(int rep, const muduo::net::TcpConnectionPtr& con);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  Resolver resolver_;
  std::string passwd_;
  std::unordered_map<muduo::string, conState> con_states_;
  std::unordered_map<muduo::string, TunnelPtr> tunnels_;
  double dns_timeout_; 
  double tunnel_timeout_;
};

}