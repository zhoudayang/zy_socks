#include "local_server.h"
#include "send_thread_pool.h"

#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>

using namespace zy;

int main()
{
  LOG_INFO << "pid = " << ::getpid();

  muduo::net::EventLoop loop;

  PoolPtr pool = std::make_shared<send_thread_pool>(&loop);

  local_server server(&loop, muduo::net::InetAddress(8788, true), muduo::net::InetAddress(8766, true), pool, "helloworld");
  server.start();

  loop.loop();
}