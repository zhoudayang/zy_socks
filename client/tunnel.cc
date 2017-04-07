#include "tunnel.h"
#include "packet.h"

#include <snappy.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <server.pb.h>

using namespace zy;
Tunnel::Tunnel(muduo::net::EventLoop *loop,
               const muduo::net::InetAddress remote_addr,
               const std::string &domain_name,
               uint16_t port,
               const std::string &passwd,
               const Tunnel::TcpConnectionPtr &con)
  : loop_(loop),
    client_(loop_, remote_addr, "tunnel_client"),
    serverCon_(con),
    domain_name_(domain_name),
    port_(port),
    passwd_(passwd),
    timerId_(),
    state_(kInit),
    timeout_(6)
{

}

void Tunnel::setup() {
  client_.setConnectionCallback(boost::bind(&Tunnel::onConnection, this, _1));
  client_.setMessageCallback(boost::bind(&Tunnel::onMessage, this, _1, _2, _3));
  serverCon_->setHighWaterMarkCallback(boost::bind(&Tunnel::onHighWaterMarkWeak, wkTunnel(shared_from_this()), kServer, _1, _2), 1024 * 1024);
  auto timer_id = loop_->runAfter(timeout_, boost::bind(&Tunnel::onTimeoutWeak, wkTunnel(shared_from_this())));
  timerId_.reset(new muduo::net::TimerId(timer_id));
  state_ = kSetup;
}

void Tunnel::onConnection(const Tunnel::TcpConnectionPtr &con)
{
  LOG_DEBUG << con->peerAddress().toIpPort() << (con->connected() ? " up " : " down ");
  if(con->connected())
  {
    LOG_INFO << "connect to remote server successful!";
    con->setTcpNoDelay(true);
    clientCon_ = con;
    state_ = kConnected;
    msg::ClientMsg message;
    message.set_type(msg::ClientMsg_Type_REQUEST);
    auto request_ptr = message.mutable_request();
    request_ptr->set_password(passwd_);
    request_ptr->set_cmd(0x01);
    request_ptr->set_addr(domain_name_);
    request_ptr->set_port(port_);
    muduo::net::Buffer buf;
    {
      std::string compressed_str;
      auto data = message.SerializeAsString();
      snappy::Compress(data.data(), data.size(), &compressed_str);
      buf.appendInt32(compressed_str.size());
      buf.append(compressed_str.data(), compressed_str.size());
    }
    con->send(&buf);
    // set high water mark callback function
    con->setHighWaterMarkCallback(boost::bind(&Tunnel::onHighWaterMarkWeak, shared_from_this(), kClient, _1, _2), 1024 * 1024);
  }
    // password not correct, teardown
  else
  {
    teardown();
  }
}

void Tunnel::onMessage(const Tunnel::TcpConnectionPtr &con, muduo::net::Buffer *buf, muduo::Timestamp receiveTime)
{
  LOG_DEBUG << domain_name_ << " transport " << buf->readableBytes() << "bytes to local_server";
  if(state_ == kConnected)
  {
    if (buf->readableBytes() > 4 && static_cast<int32_t>(buf->readableBytes()) >= buf->peekInt32() + 4)
    {
      int32_t length = buf->readInt32();
      msg::ServerMsg serverMsg;

      if (serverMsg.ParseFromArray(buf->peek(), length) && serverMsg.type() == msg::ServerMsg_Type_RESPONSE
          && serverMsg.response().rep() == 0x00) {
        buf->retrieve(length);

        if (timerId_) {
          loop_->cancel(*timerId_);
          timerId_.reset();
        }
        auto response = serverMsg.response();
        struct response successPacket;
        successPacket.addr = response.addr();
        successPacket.port = response.port();
        serverCon_->send(&successPacket, sizeof(successPacket));
        serverCon_->setContext(con);
        serverCon_->startRead();
        if (onTransportCallback_)
          onTransportCallback_();
        state_ = kTransport;
        LOG_INFO << "built data pipe to " << domain_name_ << " : " << port_ << " successful!";
      } else {
        LOG_ERROR << "cannot built data pipe of " << domain_name_ << " : " << port_;
        buf->retrieveAll();
        send_response_and_teardown(0x01);
      }
    }
  }
  else if(state_ == kTransport)
  {
    while(buf->readableBytes() > 4 && static_cast<int32_t>(buf->readableBytes()) >= 4 + buf->peekInt32())
    {
      int32_t length = buf->readInt32();
      msg::ServerMsg serverMsg;
      if(serverMsg.ParseFromArray(buf->peek(), length) && serverMsg.type() == msg::ServerMsg_Type_DATA && !serverMsg.data().empty())
      {
        buf->retrieve(length);
        serverCon_->send(serverMsg.data().data(), serverMsg.data().size());
      }
      else
      {
        LOG_ERROR << "remote server error due to " << domain_name_;
        buf->retrieveAll();
        teardown();
        return;
      }
    }
  }
  else
  {
    LOG_ERROR << "unknown connection state " << state_;
    teardown();
  }
}

void Tunnel::send_response_and_teardown(uint8_t rep)
{
  struct response data;
  data.rep = rep;
  if(serverCon_ && serverCon_->connected())
  {
    serverCon_->send(&data, sizeof(data));
  }
  teardown();
}

void Tunnel::teardown()
{
  if(state_ != kTeardown)
  {
    state_ = kTeardown;
    client_.setConnectionCallback(muduo::net::defaultConnectionCallback);
    client_.setMessageCallback(muduo::net::defaultMessageCallback);
    if (serverCon_) {
      serverCon_->setContext(boost::any());
      serverCon_->shutdown();
    }
    clientCon_.reset();
  }
}

void Tunnel::onWriteCompleteWeak(const Tunnel::wkTunnel &tunnel,
                                 Tunnel::ServerClient which,
                                 const Tunnel::TcpConnectionPtr &con)
{
  auto tunnel_ptr = tunnel.lock();
  if(tunnel_ptr)
    tunnel_ptr->onWriteComplete(which, con);
}

void Tunnel::onHighWaterMarkWeak(const Tunnel::wkTunnel &tunnel,
                                 Tunnel::ServerClient which,
                                 const Tunnel::TcpConnectionPtr &con,
                                 size_t bytes_to_sent)
{
  auto tunnel_ptr = tunnel.lock();
  if(tunnel_ptr)
    tunnel_ptr->onHighWaterMark(which, con, bytes_to_sent);
}

void Tunnel::onTimeoutWeak(const Tunnel::wkTunnel &tunnel)
{
  auto tunnel_ptr = tunnel.lock();
  if(tunnel_ptr)
    tunnel_ptr->onTimeout();
}

void Tunnel::onTimeout()
{
  LOG_ERROR << "remote server to " << domain_name_ << " timeout";
  send_response_and_teardown(0x04);
}

void Tunnel::onHighWaterMark(Tunnel::ServerClient which, const Tunnel::TcpConnectionPtr &con, size_t bytes_to_sent)
{
  LOG_INFO << (which == kServer ? "server" : "client") << " onHighWaterMark " << con->name()
           << " bytes " << bytes_to_sent;
  if(which == kServer)
  {
    if(serverCon_->outputBuffer()->readableBytes() > 0)
    {
      clientCon_->stopRead();
      serverCon_->setWriteCompleteCallback(boost::bind(&Tunnel::onWriteCompleteWeak, shared_from_this(), kServer, _1));
    }
  }
  else
  {
    if(clientCon_->outputBuffer()->readableBytes() > 0)
    {
      serverCon_->stopRead();
      clientCon_->setWriteCompleteCallback(boost::bind(&Tunnel::onWriteCompleteWeak, shared_from_this(), kClient, _1));
    }
  }
}

void Tunnel::onWriteComplete(Tunnel::ServerClient which, const Tunnel::TcpConnectionPtr &con)
{
  LOG_INFO << (which == kServer ? "server" : "client") << " onWriteComplete " << con->name();
  if(which == kServer)
  {
    clientCon_->startRead();
    serverCon_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
  }
  else if (which == kClient)
  {
    serverCon_->startRead();
    clientCon_->setWriteCompleteCallback(muduo::net::WriteCompleteCallback());
  }
}