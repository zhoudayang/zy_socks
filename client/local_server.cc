#include "local_server.h"

#include "packet.h"
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <boost/bind.hpp>
#include <snappy.h>

using namespace zy;

local_server::local_server(muduo::net::EventLoop *loop,
                           const muduo::net::InetAddress &local_addr,
                           const muduo::net::InetAddress &remote_addr,
                           const std::string &passwd)
  : loop_(loop),
    server_(loop_, local_addr, "local_server"),
    remote_addr_(remote_addr),
    passwd_(passwd),
    tunnels_(),
    timeout_(6) // default timeout set to 6 seconds
{
  server_.setConnectionCallback(boost::bind(&local_server::onConnection, this, _1));
  server_.setMessageCallback(boost::bind(&local_server::onMessage, this, _1, _2, _3));
}

void local_server::onConnection(const muduo::net::TcpConnectionPtr &con)
{
  LOG_INFO << "connection from " << con->peerAddress().toIpPort() << " is " << (con->connected() ? " up " : " down ");
  auto name = con->name();
  if(con->connected())
  {
    tunnels_[name] = TunnelState(kStart);
    con->setTcpNoDelay(true);
  }
  else
  {
    erase_from_tunnel(name);
  }
}

void local_server::onMessage(const muduo::net::TcpConnectionPtr &con,
                             muduo::net::Buffer *buf,
                             muduo::Timestamp receiveTime)
{
  auto con_name = con->name();
  if(!tunnels_.count(con_name))
  {
    LOG_FATAL << "can't find specified connection in tunnels_";
  }
  auto& tunnel = tunnels_[con_name];
  if(tunnel.state == kStart && buf->readableBytes() > 2)
  {
    char ver = buf->peek()[0];
    if(ver != 0x05)
    {
      buf->retrieveAll();
      struct verify verifyPacket;
      verifyPacket.method = 0xff;
      con->send(&verifyPacket, sizeof(verifyPacket));
      con->shutdown();
      return;
    }
    uint8_t nmethods = buf->peek()[1];
    if(buf->readableBytes() < static_cast<size_t>(2 + nmethods))
    {
      LOG_TRACE << con_name << " methods not get all";
      return;
    }
    else
    {
      buf->retrieve(2);
      auto methods = buf->retrieveAsString(nmethods);
      char no_verify = 0x00;
      if(methods.find(no_verify) == muduo::string::npos)
      {
        LOG_INFO << con_name << " can't support no security verify!";
        buf->retrieveAll();
        struct verify verifyPacket;
        verifyPacket.method = 0xff;
        con->send(&verifyPacket, sizeof(verifyPacket));
        con->shutdown();
        return;
      }
      else
      {
        struct verify verifyPacket;
        con->send(&verifyPacket, sizeof(verifyPacket));
        tunnel.state = kVerified;
        return;
      }
    }
  }
  else if(tunnel.state == kVerified && buf->readableBytes() > 6)
  {
    char cmd = buf->peek()[1];
    if(cmd != 0x01)
    {
      buf->retrieveAll();
      struct response unsupportedCommand;
      unsupportedCommand.rep = 0x07;
      con->send(&unsupportedCommand, sizeof(unsupportedCommand));
      con->shutdown();
      return;
    }
    char atyp =  buf->peek()[3];
    if(atyp != 0x03)
    {
      buf->retrieveAll();
      struct response unsupportedAtyp;
      unsupportedAtyp.rep = 0x08;
      con->send(&unsupportedAtyp, sizeof(unsupportedAtyp));
      con->shutdown();
      return;
    }
    uint8_t domain_len = buf->peek()[4];
    if(buf->readableBytes() < static_cast<size_t>(7 + domain_len))
    {
      LOG_INFO << con_name << " domain name not complete";
      return;
    }
    else
    {
      buf->retrieve(5);
      std::string domain(buf->peek(), buf->peek() + domain_len);
      buf->retrieve(domain_len);
      uint16_t port;
      memcpy(&port, buf->peek(), sizeof(port));
      port = muduo::net::sockets::networkToHost16(port);
      buf->retrieveInt16();
      tunnel.state = kGotcmd;
      con->stopRead();
      tunnel.tunnel.reset(new Tunnel(loop_, remote_addr_, domain, port, passwd_, con));
      tunnel.tunnel->set_timeout(timeout_);
      tunnel.tunnel->set_onTransportCallback(boost::bind(&local_server::set_con_state, this, con_name, kTransport));
      tunnel.tunnel->setup();
      tunnel.tunnel->connect();
      return;
    }
  }
  else if(tunnel.state == kTransport && !con->getContext().empty())
  {
    auto clientCon = boost::any_cast<const muduo::net::TcpConnectionPtr&>(con->getContext());
    msg::ClientMsg msg_data;
    msg_data.set_type(msg::ClientMsg_Type_DATA);
    msg_data.set_data(buf->peek(), buf->readableBytes());
    buf->retrieveAll();
    muduo::net::Buffer buffer;
    {
      std::string compressed_str;
      auto data = msg_data.SerializeAsString();
      snappy::Compress(data.data(), data.size(), &compressed_str);
      buffer.appendInt32(static_cast<int32_t>(compressed_str.size()));
      buffer.append(compressed_str.data(), compressed_str.size());
    }
    clientCon->send(&buffer);
  }
  else
  {
    LOG_ERROR << "unknown connection state!";
    con->shutdown();
  }
}

void local_server::set_con_state(const muduo::string &con_name, local_server::conState state)
{
  std::unordered_map<muduo::string, TunnelState>::iterator it = tunnels_.find(con_name);
  if(it != tunnels_.end())
  {
    (it->second).state = state;
  }
}

void local_server::erase_from_tunnel(const muduo::string &con_name)
{
  auto it = tunnels_.find(con_name);
  if(it != tunnels_.end())
    tunnels_.erase(it);
}