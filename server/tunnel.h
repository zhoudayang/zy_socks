#pragma once

#include <muduo/net/TcpClient.h>
#include <boost/noncopyable.hpp>

namespace zy
{
class Tunnel : boost::noncopyable, public boost::enable_shared_from_this<Tunnel>
{
 public:

 private:
};
boost::shared_ptr<Tunnel> TunnelPtr;
}


