#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <muduo/base/LogFile.h>

#include "socks_server.h"

#include "config_json.h"

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
  g_logFile.reset(new muduo::LogFile("/tmp/zy_socks", 500 * 1024, false, 3, 100));
  muduo::Logger::setOutput(outputFunc);
  muduo::Logger::setLogLevel(muduo::Logger::WARN);
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

  double dns_timeout = config.dns_timeout();
  double timeout = config.timeout();
  std::string passwd = config.password();
  uint16_t port = config.server_port();
  bool ipv6 = config.server_ipv6();

  if(daemon(0, 0) == -1)
  {
    fprintf(stderr, "create daemon process error!");
    exit(-1);
  }

  init_log();

  LOG_INFO << "pid = " << ::getpid();

  muduo::net::EventLoop loop;
  socks_server server(&loop, muduo::net::InetAddress(port, false, ipv6), passwd);
  server.set_dns_timeout(dns_timeout);
  server.set_tunnel_timeout(timeout);
  server.start();

  loop.loop();
}
