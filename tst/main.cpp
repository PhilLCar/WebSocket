/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 03 Mar 2022
 * Description: Very simple demo for the websocket.hpp header for C++. (Use with test.html)
 */

#include <websocket.hpp>

#include <iostream>
#include <ctime>
#include <cstdio>

// Use a similar struct for your global variables
struct Env {
  ws::Connection* connection;
};

std::string dateTime() {
  std::time_t now;
  std::string theDateTime;

  std::time(&now);
  {
    std::tm* local = std::localtime(&now);
    char     buffer[256];

    std::sprintf(buffer, "[%04d/%02d/%02d %02d:%02d:%02d]",
            local->tm_year + 1900,
            local->tm_mon + 1,
            local->tm_mday,
            local->tm_hour,
            local->tm_min,
            local->tm_sec);
    theDateTime = std::string(buffer);
  }
  return theDateTime;
}

void receive(ws::Connection* connection, const ws::RawData* data) {
  if (data->type != ws::DATA_TEXT && data->type != ws::DATA_PING) {
    // Disconnect
    connection->getEnvPtr<Env>()->connection = NULL;
  } else if (data->type == ws::DATA_TEXT) {
    std::string message((char*)data->buffer, data->size);

    std::cout << dateTime() << " From client #" << connection->getClientID() << ": " << message << std::endl;
  }
}

void connect(ws::Connection* connection) {
  std::string message = "Connection established";

  connection->getEnvPtr<Env>()->connection = connection;
  std::cout << dateTime() << " From client #" << connection->getClientID() << ": " << message << std::endl;
  connection->onReceive += receive;
  connection->send(message);
}

int main(int argc, char *argv[]) {
  Env env = { NULL };
  ws::WebSocket websocket(8000, env);

  websocket.onConnect += connect;

  websocket.start();
  std::cout << websocket.message() << std::endl;
  std::cout << "To ping send 'p', to quit send 'q' string.\n";
  while (true) {
    std::string line;
    std::getline(std::cin, line);
    if (line == "q") break;
    if (env.connection) {
      if (line == "p") {
        int pingms = env.connection->ping();
        if (pingms >= 0) {
          std::cout << dateTime() << " Ping with client #" << 
                      env.connection->getClientID() << ": " << pingms << "ms\n";
        } else {
          std::cout << dateTime() << " Ping with client #" << 
                       env.connection->getClientID() << ": timed out\n";
        }
      } else {
        env.connection->send(line);
      }
    }
  }
  websocket.stop();
  return 0;
}