#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include <websocket.h>

class WebSocket {
public:
  WebSocket();
  ~WebSocket();

public:
  send();
  receive();

private:
  Interface *interface;
};

#endif