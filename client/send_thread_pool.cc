#include "send_thread_pool.h"

#include <muduo/base/Logging.h>
#include <snappy.h>
#include <boost/bind.hpp>

using namespace zy;

send_thread_pool::send_thread_pool(muduo::net::EventLoop *loop, int thread_num)
  : loop_(loop),
    pool_("send_thread_pool")
{
  pool_.start(thread_num);
}

void send_thread_pool::send_in_pool(const boost::weak_ptr<muduo::net::TcpConnection> &wkCon, const msg::ClientMsg &msg)
{
  std::string compressed_str;
  {
    auto data = msg.SerializeAsString();
    snappy::Compress(data.c_str(), data.size(), &compressed_str);
  }
  loop_->runInLoop(boost::bind(&send_thread_pool::send_in_loop, this, wkCon, compressed_str));
}

void send_thread_pool::setQueueSize(int size)
{
  pool_.setMaxQueueSize(size);
}

void send_thread_pool::send_in_loop(const boost::weak_ptr<muduo::net::TcpConnection> &wkCon, const std::string &str)
{
  auto con = wkCon.lock();
  if(con)
  {
    int32_t length = static_cast<int32_t>(str.size());
    muduo::net::Buffer buf;
    buf.appendInt32(length);
    buf.append(str.data(), str.size());
    con->send(&buf);
  }
  else
  {
    LOG_ERROR << "send_in_pool remote connection is not valid now!";
  }
}
