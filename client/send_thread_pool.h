#pragma once

#include <boost/noncopyable.hpp>
#include <boost/weak_ptr.hpp>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/ThreadPool.h>
#include <client.pb.h>

namespace zy
{
class send_thread_pool : boost::noncopyable
{
 public:
  explicit send_thread_pool(muduo::net::EventLoop* loop, int thread_num = 2);

  // thread safe
  void send_in_pool(const boost::weak_ptr<muduo::net::TcpConnection>& wkCon, const msg::ClientMsg& msg);

  void setQueueSize(int size);

  ~send_thread_pool() = default;

 private:
  void send_in_loop(const boost::weak_ptr<muduo::net::TcpConnection>& wkCon, const std::string& data);

  muduo::net::EventLoop* loop_;
  muduo::ThreadPool pool_;
};
typedef std::shared_ptr<send_thread_pool> PoolPtr;
}