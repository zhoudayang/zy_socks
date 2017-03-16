#pragma once

#include <muduo/cdns/Resolver.h>
#include <boost/noncopyable.hpp>
#include <boost/weak_ptr.hpp>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TimerId.h>

namespace zy
{
class Resolver : boost::noncopyable
{
 public:
  typedef boost::function<void(const muduo::net::TcpConnectionPtr&, const muduo::string&)> ErrorCallback;
  typedef boost::function<void(const muduo::net::TcpConnectionPtr&, const muduo::net::InetAddress&)> ResolveCallback;

  explicit Resolver(muduo::net::EventLoop* loop);

  void setResolveCallback(const ResolveCallback& resolveCb){ resolveCallback_ = resolveCb; }

  void setErrorCallback(const ErrorCallback& errorCb) { errorCallback_ = errorCb; }

  // resolve in loop, thread safe, if from weak_ptr, not from shared_ptr directly
  void resolve(const muduo::string& host, uint16_t port, const boost::weak_ptr<muduo::net::TcpConnection>& serverCon);

  void set_timeout(double timeout) { timeout_ = timeout; }

  ~Resolver() = default;

 private:

  void onResolve(muduo::net::TimerId timerId, const muduo::string& host, uint16_t port,
                 const boost::weak_ptr<muduo::net::TcpConnection>& serverCon,
                 const muduo::net::InetAddress& addr);

  void onError(const muduo::string& host, const boost::weak_ptr<muduo::net::TcpConnection>& serverCon);

  static bool is_valid(const muduo::net::InetAddress& address);

  void resolve_in_loop(muduo::net::TimerId timerId, const muduo::string& host, uint16_t port,
                       const boost::weak_ptr<muduo::net::TcpConnection>& serverCon);

  muduo::net::EventLoop* loop_;
  cdns::Resolver resolver_;
  double timeout_;
  ErrorCallback errorCallback_;
  ResolveCallback resolveCallback_;
};
}