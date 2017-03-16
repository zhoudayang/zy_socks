#pragma once

#include "tunnel.h"
#include "send_thread_pool.h"

#include <boost/noncopyable.hpp>
#include <muduo/net/TcpServer.h>
#include <unordered_map>
#include <muduo/base/ThreadPool.h>

namespace zy
{
class local_server : boost::noncopyable
{
public:
  enum conState
  {
    kStart, // 连接刚开始建立
    kVerifyed, // 已经验证通过, 正在等待命令到来
    kGotcmd, // 已经接收所有命令，正在与远程主机协调
    kTransport // 正在执行应用层数据交互
  };

  local_server(muduo::net::EventLoop* loop, const muduo::net::InetAddress& local_addr, const muduo::net::InetAddress& remote_addr,
               const PoolPtr& pool, const std::string& passwd);

  void onConnection(const muduo::net::TcpConnectionPtr& con);

  void onMessage(const muduo::net::TcpConnectionPtr& con, muduo::net::Buffer* buf, muduo::Timestamp receiveTime);

  void set_con_state(const muduo::string& con_name, conState state);
  
  void start() { server_.start(); }

  void set_timeout(double timeout) { timeout_ = timeout; }

private:

  void erase_from_con_states(const muduo::string& con_name);

  void erase_from_tunnels(const muduo::string& con_name);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  muduo::net::InetAddress remote_addr_;
  PoolPtr pool_;
  std::string password_;
  std::unordered_map<muduo::string, conState> con_states_;
  std::unordered_map<muduo::string, TunnelPtr> tunnels_;
  double timeout_;
};
}