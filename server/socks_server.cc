#include "socks_server.h"

#include <client.pb.h>
#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>
#include <snappy.h>
#include <server.pb.h>

using namespace zy;

socks_server::socks_server(muduo::net::EventLoop *loop,
                               const muduo::net::InetAddress &addr,
                               const std::string &passwd)
  : loop_(loop),
    server_(loop_, addr, "proxy_server"),
    resolver_(loop_),
    passwd_(passwd),
    con_states_(),
    tunnels_(),
    dns_timeout_(3),
    tunnel_timeout_(5)
{
  server_.setConnectionCallback(boost::bind(&socks_server::onConnection, this, _1));
  server_.setMessageCallback(boost::bind(&socks_server::onMessage, this, _1, _2, _3));
  resolver_.set_timeout(dns_timeout_);
  resolver_.setResolveCallback(boost::bind(&socks_server::onResolve, this, _1, _2));
  resolver_.setErrorCallback(boost::bind(&socks_server::onResolveError, this, _1, _2));
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
  auto& state = con_states_[con_name];
  while(buf->readableBytes() > 4 && static_cast<int32_t>(buf->readableBytes()) >=  4 + buf->peekInt32())
  {
    int32_t length = buf->readInt32();
    msg::ClientMsg message;
    {
      std::string uncompressed_str;
      {
        std::string compressed_str(buf->peek(), buf->peek() + length);
        buf->retrieve(length);
        snappy::Uncompress(compressed_str.data(), compressed_str.size(), &uncompressed_str);
      }
      if(message.ParseFromArray(uncompressed_str.data(), uncompressed_str.size()))
      {
        if(state == kTransport && message.type() == msg::ClientMsg_Type_DATA && !con->getContext().empty())
        {
          auto clientCon = boost::any_cast<const muduo::net::TcpConnectionPtr&>(con->getContext());
          clientCon->send(message.data().data(), message.data().size());
        }
        else if(state == kStart && message.type() == msg::ClientMsg_Type_REQUEST)
        {
          auto request = message.request();
          if(request.password() != passwd_)
          {
            LOG_WARN << "invalid password!";
            send_response_and_down(0x05, con);
            return;
          }
          else if(request.cmd() != 0x01)
          {
            LOG_ERROR << "unsupport command " << request.cmd();
            send_response_and_down(0x07, con);
            return;
          }
          muduo::string domain = request.addr().c_str();
          uint16_t port = static_cast<uint16_t>(request.port());
          resolver_.resolve(domain, port, boost::weak_ptr<muduo::net::TcpConnection>(con));
          // stop read now, until resolve the domain and connection to specified host
          con->stopRead();
          state = kGotcmd;
        }
        else
        {
          LOG_ERROR << "unknown connection state!";
          con->shutdown();
        }
      }
      else // parse error, close the connection immediately
      {
        LOG_ERROR << "parse from array error!";
        con->shutdown();
      }
    }
  }
}

// send response to client and shutdown the connection
void socks_server::send_response_and_down(int rep, const muduo::net::TcpConnectionPtr &con)
{
  muduo::net::Buffer msg_buf;
  {
    msg::ServerMsg response;
    response.set_type(msg::ServerMsg_Type_RESPONSE);
    auto reponse_ptr = response.mutable_response();
    reponse_ptr->set_rep(rep);
    std::string response_str = response.SerializeAsString();

    int length = static_cast<int32_t>(response_str.size());
    msg_buf.appendInt32(length);
    msg_buf.append(response_str.data(), length);
  }
  con->send(&msg_buf);
  con->shutdown();
}


void socks_server::onResolve(const muduo::net::TcpConnectionPtr &con , const muduo::net::InetAddress &addr)
{
  con_states_[con->name()] = kResolved;
  TunnelPtr tunnel(new Tunnel(loop_, addr, con));
  tunnel->set_timeout(tunnel_timeout_);
  tunnel->setOnConnectionCallback(boost::bind(&socks_server::set_con_state, this, con->name(), kTransport));
  tunnel->setup();
  tunnel->connect();
  tunnels_[con->name()] = tunnel;
}

void socks_server::onResolveError(const muduo::net::TcpConnectionPtr &con, const muduo::string &host)
{
  LOG_INFO << "onResolveError due to resolve " << host;
  send_response_and_down(0x03, con);
}

void socks_server::set_con_state(const muduo::string &name, socks_server::conState state) 
{
  if(con_states_.count(name))
    con_states_[name] = state;
}

