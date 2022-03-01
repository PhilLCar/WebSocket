#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include <cstddef>
#include <string>

#include <websocket.h>

class WebSocket {
public:
  WebSocket(const int port, const int timeout = 30000, const int mask = 0);
  ~WebSocket();

public:
  template <typename T>
  void send(T &object);

  void send(void *data, size_t size);
  void send(const char *text);
  void send(const std::string &text);

  template <typename T>
  T receive();


private:
  Interface *interface;
};

#endif