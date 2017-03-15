#include "socks_server.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>

using namespace zy;

socks_server::socks_server(muduo::net::EventLoop *loop,
                               const muduo::net::InetAddress &addr,
                               const std::string &passwd)
  : loop_(loop),
    server_(loop_, addr, "proxy_server"),
    resolver_(loop_),
    passwd_(passwd),
    con_states_(),
    tunnels_()
{
  server_.setConnectionCallback(boost::bind(&socks_server::onConnection, this, _1));
  server_.setMessageCallback(boost::bind(&socks_server::onMessage, this, _1, _2, _3));
  // todo : resolver
}

void socks_server::onConnection(const muduo::net::TcpConnectionPtr &con)
{
  auto con_name = con->name();
  LOG_DEBUG << con_name << (con->connected() ? " up" : " down");
  if(con->connected())
  {
    con->setTcpNoDelay(true);
    con_states_[con_name] = kStart;
  }
  else
  {
    erase_from_con_states(con_name);
    erase_from_tunnels(con_name);
  }
}

void socks_server::erase_from_con_states(const muduo::string &con_name)
{
  auto it = con_states_.find(con_name);
  if(it != con_states_.end())
    con_states_.erase(it);
}

void socks_server::erase_from_tunnels(const muduo::string &con_name)
{
  auto it = tunnels_.find(con_name);
  if(it != tunnels_.end())
    tunnels_.erase(it);
}

void socks_server::onMessage(const muduo::net::TcpConnectionPtr &con,
                             muduo::net::Buffer *buf,
                             muduo::Timestamp receiveTime)
{
  auto con_name = con->name();
  if(!con_states_.count(con_name))
  {
    LOG_FATAL << "can't find specified connection in con_states " << con_name;
  }
  while(buf->readableBytes() > 4 && static_cast<int32_t>(buf->readableBytes()) >=  4 + buf->peekInt32())
  {

  }
}
