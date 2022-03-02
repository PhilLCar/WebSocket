#include <websocket.hpp>

#include <iostream>

struct Env {

};

void connect(WebSocket* socket, const int client) {
  socket->send(client, "Connected, woohoo!");
}

void receive(WebSocket* socket, const int client, const WebSocket::RawData* data) {
  std::cout << data->buffer << "\n";
  
}

int main() {
  Env env;
  WebSocket websocket(8000);

  websocket.environmentPointer = (void*)&env;
  websocket.onConnect += connect;
  websocket.onReceive += receive;

  websocket.start();

  websocket.stop();

  std::cout << "Press <enter> to continue...";
  std::getline();
}