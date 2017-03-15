#include "Resolver.h"

#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>

using namespace zy;

Resolver::Resolver(muduo::net::EventLoop *loop)
    : loop_(loop),
      resolver_(loop_, cdns::Resolver::kDNSonly),
      timeout_(3) // set default dns resolve timeout to 3 seconds
{

}

void Resolver::resolve(const muduo::string &host, uint16_t port, const boost::weak_ptr<muduo::net::TcpConnection>& serverCon)
{
  // 设置超时回调函数
  auto timerId = loop_->runAfter(timeout_, boost::bind(&Resolver::onError, this, host, serverCon));
  loop_->runInLoop(boost::bind(&Resolver::resolve_in_loop, this, timerId, host, port, serverCon));
}

void Resolver::onResolve(muduo::net::TimerId timerId,
                         const muduo::string &host,
                         uint16_t port,
                         const boost::weak_ptr<muduo::net::TcpConnection> &serverCon,
                         const muduo::net::InetAddress &addr)
{
  muduo::net::TcpConnectionPtr con = serverCon.lock();
  loop_->cancel(timerId);
  if(!con)
  {
    LOG_WARN << "Resolver::onResolve lost client connection, the resolve host is " << host;
  }
  else if(!is_valid(addr))
  {
    LOG_ERROR << "resolve address of " << host << " is not valid";
    if(errorCallback_)
      errorCallback_(con, host);
  }
  else if(resolveCallback_)
  {
    muduo::net::InetAddress serverAddr(addr.toIp(), port);
    resolveCallback_(con, serverAddr);
  }
}

void Resolver::onError(const muduo::string &host, const boost::weak_ptr<muduo::net::TcpConnection> &serverCon)
{
  LOG_INFO << "resolve timeout to " << host;
  auto conn = serverCon.lock();
  if(!conn)
  {
    LOG_WARN << "Resolver::onError lost client connection, the resolve host is " << host;
  }
  else if(errorCallback_)
  {
    errorCallback_(conn, host);
  }
}

bool Resolver::is_valid(const muduo::net::InetAddress &address) {
  return address.ipNetEndian() != INADDR_ANY;
}

void Resolver::resolve_in_loop(muduo::net::TimerId timerId,
                               const muduo::string &host,
                               uint16_t port,
                               const boost::weak_ptr<muduo::net::TcpConnection>& serverCon)
{
  resolver_.resolve(host, boost::bind(&Resolver::onResolve,
                    this, timerId, host, port, serverCon, _1));
}

