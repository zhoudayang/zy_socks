#include "local_server.h"
#include "config_json.h"

#include <muduo/net/EventLoop.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h>

using namespace zy;

std::unique_ptr<muduo::LogFile> g_logFile;

void outputFunc(const char* msg, int len)
{
  g_logFile->append(msg, len);
}

void flushFunc()
{
  g_logFile->flush();
}

void init_log()
{
  g_logFile.reset(new muduo::LogFile("/tmp/local_server", 500 * 1024, false, 3, 100));
  muduo::Logger::setOutput(outputFunc);
  muduo::Logger::setLogLevel(muduo::Logger::INFO);
  muduo::Logger::setFlush(flushFunc);
}

int main(int argc, char* argv[])
{
  if(argc != 2)
  {
    fprintf(stderr, "Usage: %s config_path", ::basename(argv[0]));
    exit(-1);
  }

  config_json config(argv[1]);

  std::string server_ip = config.server_addr();
  bool server_ipv6 = config.server_ipv6();
  uint16_t server_port = config.server_port();
  muduo::net::InetAddress server_addr(server_ip.c_str(), server_port, server_ipv6);

  std::string local_ip = config.local_addr();
  uint16_t local_port = config.local_port();
  muduo::net::InetAddress local_addr(local_ip.c_str(), local_port);

  double timeout = config.timeout();
  std::string passwd = config.password();

  if(daemon(0, 0) == -1)
  {
    fprintf(stderr, "create daemon process error!");
    exit(-1);
  }

  init_log();

  LOG_INFO << " pid = " << ::getpid();
  muduo::net::EventLoop loop;

  PoolPtr pool = std::make_shared<send_thread_pool>(&loop);
  local_server server(&loop, local_addr, server_addr, pool, passwd);
  server.set_timeout(timeout);

  server.start();

  loop.loop();
}