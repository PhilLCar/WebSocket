#include <websocket.hpp>

WebSocket::WebSocket(const int port, const int timeout, const int mask) {
  interface = startservice(port, timeout, mask);
}

WebSocket::~WebSocket() {
  stopservice(interface);
}

template <typename T>
void WebSocket::send(T &object) {

}