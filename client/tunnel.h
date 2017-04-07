#pragma once

#include <boost/noncopyable.hpp>
#include <memory>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TimerId.h>
#include <client.pb.h>

namespace zy
{
class Tunnel : boost::noncopyable, public std::enable_shared_from_this<Tunnel>
{
 public:
  typedef muduo::net::TcpConnectionPtr TcpConnectionPtr;
  typedef std::weak_ptr<Tunnel> wkTunnel;
  typedef boost::function<void()> onTransportCallback;

  enum State
  {
    kInit,
    kSetup,
    kConnected,
    kTransport,
    kTeardown
  };

  Tunnel(muduo::net::EventLoop* loop, const muduo::net::InetAddress remote_addr,
         const std::string& domain_name, uint16_t port,
         const std::string& passwd, const TcpConnectionPtr& con);

  ~Tunnel() = default;

  void onConnection(const TcpConnectionPtr& con);

  void onMessage(const TcpConnectionPtr& con, muduo::net::Buffer* buf, muduo::Timestamp receiveTime);

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

  void onWriteComplete(ServerClient which, const TcpConnectionPtr& con);

  void onHighWaterMark(ServerClient which, const TcpConnectionPtr& con, size_t bytes_to_sent);

  void onTimeout();

  static void onWriteCompleteWeak(const wkTunnel& tunnel, ServerClient which, const TcpConnectionPtr& con);

  static void onHighWaterMarkWeak(const wkTunnel& tunnel, ServerClient which, const TcpConnectionPtr& con, size_t bytes_to_sent);

  static void onTimeoutWeak(const wkTunnel& tunnel);

  void send_response_and_teardown(uint8_t rep);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpClient client_;
  TcpConnectionPtr serverCon_;
  TcpConnectionPtr clientCon_;
  std::string domain_name_;
  uint16_t port_;
  std::string passwd_;
  std::unique_ptr<muduo::net::TimerId> timerId_;
  State state_;
  double timeout_;
  onTransportCallback onTransportCallback_;
};
typedef std::shared_ptr<Tunnel> TunnelPtr;
}
