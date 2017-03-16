#include "tunnel.h"

#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <server.pb.h>

using namespace zy;

Tunnel::Tunnel(muduo::net::EventLoop *loop,
               const muduo::net::InetAddress &addr,
               const muduo::net::TcpConnectionPtr &serverCon)
  : loop_(loop),
    client_(loop_, addr, "proxy_client"),
    serverCon_(serverCon),
    timerId_(),
    host_addr_(addr.toIpPort()),
    timeout_(5) // default timeout is 5 second
{

}

Tunnel::~Tunnel()
{
  LOG_INFO << "~Tunnel";
}

void Tunnel::onClientConnection(const muduo::net::TcpConnectionPtr &con)
{
  LOG_DEBUG << (con->connected() ? "up" : "down");
  if(con->connected())
  {
    LOG_INFO << "proxy built! " << serverCon_->peerAddress().toIpPort() << " <-> " << con->peerAddress().toIpPort();
    if(timerId_)
    {
      loop_->cancel(*timerId_);
      timerId_.reset();
    }
    con->setTcpNoDelay(true);
    con->setHighWaterMarkCallback(boost::bind(
        &Tunnel::onHighWaterMarkWeak, boost::weak_ptr<Tunnel>(shared_from_this()), kClient, _1, _2), 1024 * 1024);
    muduo::net::Buffer msg_buf;
    {
      msg::ServerMsg serverMsg;
      serverMsg.set_type(msg::ServerMsg_Type_RESPONSE);
      auto response_ptr = serverMsg.mutable_response();
      response_ptr->set_rep(0x00);
      response_ptr->set_addr(con->localAddress().ipNetEndian());
      response_ptr->set_port(con->localAddress().portNetEndian());
      auto message_str = serverMsg.SerializeAsString();
      int32_t length = static_cast<int32_t>(message_str.size());
      msg_buf.appendInt32(length);
      msg_buf.append(message_str.data(), length);
    }
    serverCon_->send(&msg_buf);
    serverCon_->setContext(con);
    serverCon_->startRead();
    clientCon_ = con;
    if(onConnectionCallback_)
      onConnectionCallback_();
  }
  else
  {
    teardown();
  }
}

void Tunnel::onClientMessage(const muduo::net::TcpConnectionPtr &con, muduo::net::Buffer *buf, muduo::Timestamp)
{
  LOG_DEBUG << "message from remote server " << con->peerAddress().toIpPort() << " " << buf->readableBytes();
  muduo::net::Buffer msg_buf;
  {
    msg::ServerMsg serverMsg;
    serverMsg.set_type(msg::ServerMsg_Type_DATA);
    auto data_ptr = serverMsg.mutable_data();
    data_ptr->assign(buf->peek(), buf->peek() + buf->readableBytes());
    buf->retrieveAll();
    auto message_str = serverMsg.SerializeAsString();
    int32_t length = static_cast<int32_t>(message_str.size());
    msg_buf.appendInt32(length);
    msg_buf.append(message_str.data(), length);
  }
  serverCon_->send(&msg_buf);
}

void Tunnel::setup()
{
  client_.setConnectionCallback(boost::bind(&Tunnel::onClientConnection, this, _1));
  client_.setMessageCallback(boost::bind(&Tunnel::onClientMessage, this, _1, _2, _3));
  serverCon_->setHighWaterMarkCallback(boost::bind(&Tunnel::onHighWaterMarkWeak, boost::weak_ptr<Tunnel>(shared_from_this()),
                                                   kServer, _1, _2), 1024 * 1024);
  auto timer = loop_->runAfter(timeout_, boost::bind(&Tunnel::onTimeoutWeak, boost::weak_ptr<Tunnel>(shared_from_this())));
  timerId_.reset(new muduo::net::TimerId(timer));
}

void Tunnel::teardown()
{
  client_.setConnectionCallback(muduo::net::defaultConnectionCallback);
  client_.setMessageCallback(muduo::net::defaultMessageCallback);
  if(serverCon_)
  {
    serverCon_->setContext(boost::any());
    serverCon_->shutdown();
  }
  clientCon_.reset();
}

void Tunnel::onTimeout()
{
  if(serverCon_)
  {
    LOG_WARN << "proxy_client of address " << serverCon_->peerAddress().toIp() << " to " << host_addr_ << " connect timeout";
    client_.stop();

    muduo::net::Buffer msg_buf;
    {
      msg::ServerMsg serverMsg;
      serverMsg.set_type(msg::ServerMsg_Type_RESPONSE);
      auto response_ptr = serverMsg.mutable_response();
      response_ptr->set_rep(0x04);
      auto message_str = serverMsg.SerializeAsString();
      int32_t length = static_cast<int32_t>(message_str.size());
      msg_buf.appendInt32(length);
      msg_buf.append(message_str.data(), length);
    }

    serverCon_->send(&msg_buf);
    serverCon_->shutdown();
  }
  else
  {
    LOG_ERROR << "serverCon_ is not valid now!";
  }
}

void Tunnel::onHighWaterMark(Tunnel::ServerClient which,
                             const muduo::net::TcpConnectionPtr &con,
                             size_t bytes_to_sent)
{
  LOG_INFO << (which == kServer ? "server" : "client")
           << " onHighWaterMark " << con->name() << " bytes " << bytes_to_sent;
  if(which == kServer)
  {
    // 只关心发送的那个方向
    if(serverCon_->outputBuffer()->readableBytes() > 0)
    {
      clientCon_->stopRead();
      serverCon_->setWriteCompleteCallback(boost::bind(&Tunnel::onWriteCompleteWeak,
          boost::weak_ptr<Tunnel>(shared_from_this()), kServer, _1));
    }
  }
  else
  {
    if(clientCon_->outputBuffer()->readableBytes() > 0)
    {
      serverCon_->stopRead();
      clientCon_->setWriteCompleteCallback(boost::bind(&Tunnel::onWriteCompleteWeak,
          boost::weak_ptr<Tunnel>(shared_from_this()), kClient, _1));
    }
  }
}

void Tunnel::onWriteComplete(Tunnel::ServerClient which, const muduo::net::TcpConnectionPtr &con)
{
  LOG_INFO << (which == kServer ? "server" : "client")
           << " onWriteComplete " << con->name();
  if(which == kServer)
  {
    clientCon_->startRead();
    serverCon_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
  }
  else
  {
    serverCon_->startRead();
    clientCon_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
  }
}

void Tunnel::onHighWaterMarkWeak(const boost::weak_ptr<Tunnel> &wkTunnel,
                                 Tunnel::ServerClient which,
                                 const muduo::net::TcpConnectionPtr &con,
                                 size_t bytes_to_sent)
{
  auto tunnel = wkTunnel.lock();
  if(tunnel)
    tunnel->onHighWaterMark(which, con, bytes_to_sent);
}

void Tunnel::onWriteCompleteWeak(const boost::weak_ptr<Tunnel> &wkTunnel,
                                 Tunnel::ServerClient which,
                                 const muduo::net::TcpConnectionPtr &con)
{
  auto tunnel = wkTunnel.lock();
  if(tunnel)
    tunnel->onWriteComplete(which, con);
}

void Tunnel::onTimeoutWeak(const boost::weak_ptr<Tunnel> &wkTunnel)
{
  auto tunnel = wkTunnel.lock();
  if(tunnel)
    tunnel->onTimeout();
}
