#include <websocket.hpp>

#include <iostream>

struct Env {

};

void receive(ws::Connection* connection, const ws::RawData* data) {
  std::cout << data->buffer << "\n";
}

void connect(ws::Connection* connection) {
  connection->send("Connected, woohoo!");
  connection->onReceive += receive;
}

int main(int argc, char *argv[]) {
  Env env;
  ws::WebSocket websocket(8000);

  websocket.environmentPointer = (void*)&env;
  websocket.onConnect += connect;

  websocket.start();

  system("pause");

  websocket.stop();

  return 0;
}