#pragma once

namespace zy
{
struct verify
{
  char ver = 0x05;
  char method = 0x00;
}__attribute__((__packed__));

struct response
{
  char ver = 0x05;
  char rep = 0x00;
  char rev = 0x00;
  char atyp = 0x01;
  uint32_t addr = 0;
  uint16_t port = 0;
}__attribute__((__packed__));

static_assert(sizeof(verify) == 2, "verify packed error");
static_assert(sizeof(response) == 10, "response packed error");
}