#include "local_server.h"
#include "packet.h"

#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <boost/bind.hpp>

using namespace zy;

local_server::local_server(muduo::net::EventLoop *loop,
                           const muduo::net::InetAddress &local_addr,
                           const muduo::net::InetAddress &remote_addr,
                           const std::string &passwd)
    : loop_(loop),
      server_(loop_, local_addr, "local_server"),
      remote_addr_(remote_addr),
      password_(passwd),
      con_states_(),
      tunnels_(),
      timeout_(6) // set default timeout to 6
{
  server_.setConnectionCallback(boost::bind(&local_server::onConnection, this, _1));
  server_.setMessageCallback(boost::bind(&local_server::onMessage, this, _1, _2, _3));
}

//　if disconnect from local clients, remove from tunnels
void local_server::onConnection(const muduo::net::TcpConnectionPtr &con)
{
  LOG_INFO << "connection from " << con->peerAddress().toIpPort() << " is " << (con->connected() ? " up " : " down ");
  auto name = con->name();
  if (con->connected()) {
    con_states_[name] = kStart;
    con->setTcpNoDelay(true);
  }
  else
  {
    erase_from_con_states(name);
    erase_from_tunnels(name);
  }
}

void local_server::onMessage(const muduo::net::TcpConnectionPtr &con,
                             muduo::net::Buffer *buf,
                             muduo::Timestamp receiveTime)
{
  auto con_name = con->name();
  if(!con_states_.count(con_name))
  {
    LOG_FATAL << "can't find specified connection in con_states_";
  }
  if(con_states_[con_name] == kStart && buf->readableBytes() > 2)
  {
    char ver = buf->peek()[0];
    if(ver != 0x05)
    {
      buf->retrieveAll();
      struct verify verifyPacket;
      verifyPacket.method = 0xFF;
      con->send(&verifyPacket, sizeof(verifyPacket));
      con->shutdown();
      return;
    }
    uint8_t  nmethods = buf->peek()[1];
    if(buf->readableBytes() < static_cast<size_t>(2 + nmethods))
    {
      LOG_TRACE << con_name << " methods not receive complete!";
    }
    else
    {
      buf->retrieve(2);
      auto methods = buf->retrieveAsString(nmethods);
      char no_verify = 0x00;
      if(methods.find(no_verify) == muduo::string::npos)
      {
        LOG_INFO << con_name << "can't support no security verify!";
        struct verify verifyPacket;
        verifyPacket.method = 0xFF;
        con->send(&verifyPacket, sizeof(verifyPacket));
        con->shutdown();
      }
      else
      {
        struct verify verifyPass;
        con->send(&verifyPass, sizeof(verifyPass));
        con_states_[con->name()] = kVerifyed;
      }
    }
  }
  else if(con_states_[con_name] == kVerifyed && buf->readableBytes() > 6)
  {
    char cmd = buf->peek()[1];
    if(cmd != 0x01)
    {
      buf->retrieveAll();
      struct response unsupportCommand;
      unsupportCommand.rep = 0x07;
      con->send(&unsupportCommand, sizeof(unsupportCommand));
      con->shutdown();
      return;
    }
    char atyp = buf->peek()[3];
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
      LOG_INFO << con_name << " domain name not complete yet!";
    }
    else
    {
      buf->retrieve(5);
      std::string domain = buf->retrieveAsString(domain_len).c_str();
      uint16_t port;
      memcpy(&port, buf->peek(), sizeof(uint16_t));
      port = muduo::net::sockets::networkToHost16(port);
      buf->retrieveInt16();
      con_states_[con_name] = kGotcmd;
      // stop read now
      con->stopRead();
      TunnelPtr tunnel(new Tunnel(loop_, remote_addr_, domain, port, password_, con));
      tunnel->set_timeout(timeout_);
      tunnel->set_onTransportCallback(boost::bind(&local_server::set_con_state, this, con_name, kTransport));
      tunnel->setup();
      tunnel->connect();
      tunnels_[con_name] = tunnel;
    }
  }
  else if(con_states_[con_name] == kTransport && !con->getContext().empty())
  {
    auto clientCon = boost::any_cast<const muduo::net::TcpConnectionPtr&>(con->getContext());
    msg::ClientMsg data;
    data.set_type(msg::ClientMsg_Type_DATA);
    data.set_data(buf->peek(), buf->readableBytes());
    buf->retrieveAll();
    // send data to remote socks server
    Tunnel::send_msg(clientCon, data);
  }
  else
  {
    LOG_ERROR << "unknown connection state!";
    con->shutdown();
  }
}

void local_server::erase_from_con_states(const muduo::string &con_name) {
  auto it = con_states_.find(con_name);
  if (it != con_states_.end())
    con_states_.erase(it);
}

void local_server::erase_from_tunnels(const muduo::string &con_name) {
  auto it = tunnels_.find(con_name);
  if (it != tunnels_.end())
    tunnels_.erase(it);
}

void local_server::set_con_state(const muduo::string &con_name, local_server::conState state)
{
  if(con_states_.count(con_name))
    con_states_[con_name] = state;
}
