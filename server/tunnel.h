#pragma once

#include <muduo/net/TcpClient.h>
#include <boost/noncopyable.hpp>
#include <muduo/net/TimerId.h>

namespace zy
{
class Tunnel : boost::noncopyable, public boost::enable_shared_from_this<Tunnel>
{
 public:
  typedef boost::function<void()> onConnectionCallback;

  Tunnel(muduo::net::EventLoop* loop,
         const muduo::net::InetAddress& addr,
         const muduo::net::TcpConnectionPtr& serverCon);

  ~Tunnel();

  void setOnConnectionCallback(const onConnectionCallback& cb)
  {
    onConnectionCallback_ = cb;
  }

  void onClientConnection(const muduo::net::TcpConnectionPtr& con);

  void onClientMessage(const muduo::net::TcpConnectionPtr& con, muduo::net::Buffer* buf, muduo::Timestamp);

  void set_timeout(double timeout) { timeout_ = timeout; }

  void setup();

  void connect() { client_.connect(); }

 private:
  enum ServerClient
  {
    kServer,
    kClient
  };

  void teardown();

  void onHighWaterMark(ServerClient which, const muduo::net::TcpConnectionPtr& con, size_t bytes_to_sent);

  void onWriteComplete(ServerClient which, const muduo::net::TcpConnectionPtr& con);

  void onTimeout();

  static void onHighWaterMarkWeak(const boost::weak_ptr<Tunnel>& wkTunnel, ServerClient which,
                             const muduo::net::TcpConnectionPtr& con, size_t bytes_to_sent);

  static void onWriteCompleteWeak(const boost::weak_ptr<Tunnel>& wkTunnel, ServerClient which,
                                   const muduo::net::TcpConnectionPtr& con);

  static void onTimeoutWeak(const boost::weak_ptr<Tunnel>& wkTunnel);

  muduo::net::EventLoop* loop_;
  muduo::net::TcpClient client_;
  muduo::net::TcpConnectionPtr serverCon_;
  muduo::net::TcpConnectionPtr clientCon_;
  onConnectionCallback onConnectionCallback_;
  std::unique_ptr<muduo::net::TimerId> timerId_;
  muduo::string host_addr_;
  double timeout_;
};
typedef boost::shared_ptr<Tunnel> TunnelPtr;
}


