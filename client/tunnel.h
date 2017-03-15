#pragma once

#include "send_thread_pool.h"
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TimerId.h>

namespace zy
{
class Tunnel : boost::noncopyable, public boost::enable_shared_from_this<Tunnel>
{
 public:
  typedef muduo::net::TcpConnectionPtr TcpConnectionPtr;
  typedef boost::function<void()> onTransportCallback;
  enum State
  {
    kInit,
    kSetup,
    kConnected,
    kTransport,
    kTeardown
  };

  Tunnel(muduo::net::EventLoop* loop, const muduo::net::InetAddress& remote_addr,
         PoolPtr pool, const std::string& domain_name, uint16_t port,
         const std::string& passwd, const TcpConnectionPtr& con);

  ~Tunnel() = default;

  void onConnection(const muduo::net::TcpConnectionPtr& con);

  void onMessage(const TcpConnectionPtr& con, muduo::net::Buffer*& buf, muduo::Timestamp receiveTime);

  void connect() { client_.connect(); }

  void setup();

  void teardown();

  void set_timeout(double timeout) { timeout_ = timeout; }

  void set_onTransportCallback(const onTransportCallback& cb) { onTransportCallback_ = cb; }

 private:
  enum ServerClient
  {
    kServer,
    kClient
  };

  typedef boost::weak_ptr<Tunnel> wkTunnel;

  void onWriteComplete(ServerClient which, const TcpConnectionPtr& con);

  void onHighWaterMark(ServerClient which, const TcpConnectionPtr& con, size_t bytes_to_sent);

  void onTimeout();

  static void onWriteCompleteWeak(const wkTunnel& tunnel, ServerClient which, const TcpConnectionPtr& con);

  static void onHighWaterMarkWeak(const wkTunnel& tunnel, ServerClient which, const TcpConnectionPtr& con, size_t bytes_to_sent);

  static void onTimeoutWeak(const wkTunnel& tunnel);

  void send_response_and_teardown(uint8_t rep);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpClient client_;
  // connection to proxy client
  TcpConnectionPtr serverCon_;
  // connection to remote server
  TcpConnectionPtr clientCon_;
  PoolPtr pool_;
  std::string domain_name_;
  uint16_t port_;
  std::string password_;
  std::unique_ptr<muduo::net::TimerId> timerId_;
  State state_;
  double timeout_; // timeout, default value is 6 seconds
  onTransportCallback onTransportCallback_;
};
typedef boost::shared_ptr<Tunnel> TunnelPtr;
}