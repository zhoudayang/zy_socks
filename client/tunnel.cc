#include "tunnel.h"
#include "packet.h"

#include <server.pb.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>
#include <client.pb.h>

using namespace zy;

  Tunnel::Tunnel(muduo::net::EventLoop* loop, const muduo::net::InetAddress& remote_addr,
                 PoolPtr pool, const std::string& domain_name, uint16_t port,
                 const std::string& passwd, const TcpConnectionPtr& con)
  : loop_(loop),
    client_(loop_, remote_addr, "tunnel client"),
    serverCon_(con),
    pool_(pool),
    domain_name_(domain_name),
    port_(port),
    password_(passwd),
    timerId_(),
    state_(kInit),
    timeout_(6)
{

}

void Tunnel::onConnection(const muduo::net::TcpConnectionPtr &con)
{
  LOG_DEBUG << client_.name() << (con->connected() ? " connected" : " disconnected");
  if(con->connected())
  {
    LOG_INFO << "connect to remote proxy server successful!";
    con->setTcpNoDelay(true);
    clientCon_ = con;
    state_ = kConnected;
    msg::ClientMsg message;
    message.set_type(msg::ClientMsg_Type_REQUEST);
    auto request_ptr = message.mutable_request();
    request_ptr->set_password(password_);
    request_ptr->set_cmd(0x01);
    request_ptr->set_addr(domain_name_);
    request_ptr->set_port(port_);
    pool_->send_in_pool(boost::weak_ptr<muduo::net::TcpConnection>(con), message);
  }
    // password not valid
  else if(!con->connected() && state_ == kConnected)
  {
    send_response_and_teardown(0x01);
  }
  else
  {
    teardown();
  }
}

void Tunnel::onMessage(const Tunnel::TcpConnectionPtr &con, muduo::net::Buffer *&buf, muduo::Timestamp receiveTime)
{
  LOG_DEBUG << con->name() << " " << buf->readableBytes();
  if(state_ == kConnected)
  {
    if(buf->readableBytes() > 4 && static_cast<int32_t>(buf->readableBytes()) >= buf->peekInt32() + 4)
    {
      int32_t length = buf->readInt32();
      msg::ServerMsg serverMsg;
      if(serverMsg.ParseFromArray(buf->peek(), length))
      {
        if(serverMsg.type() == msg::ServerMsg_Type_RESPONSE)
        {
          auto response = serverMsg.response();
          if(response.rep() == 0x00)
          {
            LOG_INFO << con->localAddress().toIpPort() << " built data pipeline to " << domain_name_ << " : " << port_ << " successful!";
            state_ = kTransport;
            if(timerId_)
            {
              loop_->cancel(*timerId_);
              timerId_.reset();
            }
            struct response successPacket;
            successPacket.addr = response.addr();
            successPacket.port = response.port();
            serverCon_->send(&successPacket, sizeof(successPacket));
            serverCon_->setContext(con);
            serverCon_->startRead();
            // now begin to transport between remote server and local server
            if(onTransportCallback_)
              onTransportCallback_();
          }
          else if(response.rep() == 0x03)
          {
            LOG_ERROR << "resolve error at remote server due to resolve " << domain_name_;
            send_response_and_teardown(0x03);
          }
          else if(response.rep() == 0x04)
          {
            LOG_ERROR << "connect timeout at remote server due to " << domain_name_;
            send_response_and_teardown(0x04);
          }
          else if(response.rep() == 0x05)
          {
            LOG_ERROR << "password not correct!";
            send_response_and_teardown(0x05);
          }
          else if(response.rep() == 0x07)
          {
            LOG_ERROR << "password not correct!";
            send_response_and_teardown(0x07);
          }
          else
          {
            LOG_ERROR << "unknown error at remote server!";
            send_response_and_teardown(0x01);
          }
        }
        else
        {
          LOG_ERROR << "invalid message type!";
          send_response_and_teardown(0x01);
        }
      }
      else
      {
        LOG_ERROR << "parse from array error!";
        send_response_and_teardown(0x01);
      }
      buf->retrieve(length);
    }
  }
  else if(state_ == kTransport)
  {
    while(buf->readableBytes() > 4 && static_cast<int32_t>(buf->readableBytes()) >= 4 + buf->peekInt32())
    {
      int32_t length = buf->readInt32();
      msg::ServerMsg serverMsg;
      if(serverMsg.ParseFromArray(buf->peek(), length))
      {
        if(serverMsg.type() == msg::ServerMsg_Type_DATA)
        {
          if(serverMsg.data().empty())
          {
            LOG_FATAL << "empty data from remote server!";
          }
          serverCon_->send(serverMsg.data().data(), serverMsg.data().size());
        }
        else
        {
          LOG_ERROR << "wrong message type from remote server!";
          teardown();
        }
      }
      else
      {
        LOG_ERROR << "serverMsg parse error!";
        teardown();
      }
      buf->retrieve(length);
    }
  }
  else if (state_ != kTeardown)
  {
    LOG_ERROR << "invalid tunnel state " << state_;
    teardown();
  }
}

void Tunnel::send_response_and_teardown(uint8_t rep)
{
  struct response data;
  data.rep = rep;
  serverCon_->send(&data, sizeof(data));
  teardown();
}

void Tunnel::setup()
{
  client_.setConnectionCallback(boost::bind(&Tunnel::onConnection, this, _1));
  client_.setMessageCallback(boost::bind(&Tunnel::onMessage, this, _1, _2, _3));
  serverCon_->setHighWaterMarkCallback(boost::bind(&Tunnel::onHighWaterMarkWeak,
    wkTunnel(shared_from_this()), kServer, _1, _2), 1024 * 1024);
  auto timer_id = loop_->runAfter(timeout_, boost::bind(&Tunnel::onTimeoutWeak, wkTunnel(shared_from_this())));
  timerId_.reset(new muduo::net::TimerId(timer_id));
  state_ = kSetup;
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
  state_ = kTeardown;
}

void Tunnel::onWriteComplete(Tunnel::ServerClient which, const Tunnel::TcpConnectionPtr &con) {
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

void Tunnel::onHighWaterMark(Tunnel::ServerClient which, const Tunnel::TcpConnectionPtr &con, size_t bytes_to_sent)
{
  LOG_INFO << (which == kServer ? "server" : "client") << " onHighWaterMark " << con->name()
           << " bytes " << bytes_to_sent;
  if(which == kServer)
  {
    if(serverCon_->outputBuffer()->readableBytes() > 0)
    {
      clientCon_->stopRead();
      serverCon_->setWriteCompleteCallback(boost::bind(
          &Tunnel::onWriteCompleteWeak, wkTunnel(shared_from_this()), kServer, _1));
    }
  }
  else
  {
    if(clientCon_->outputBuffer()->readableBytes() > 0)
    {
      serverCon_->stopRead();
      clientCon_->setWriteCompleteCallback(boost::bind(&Tunnel::onWriteCompleteWeak, wkTunnel(shared_from_this()), kClient, _1));
    }
  }
}

void Tunnel::onTimeout()
{
  LOG_ERROR << "proxy client of address " << serverCon_->name() << " to remote server timeout";
  client_.stop();
  send_response_and_teardown(0x04);
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