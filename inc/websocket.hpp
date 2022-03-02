/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 02 Mar 2022
 * Description: WebSocket implementation for C++.
 */

#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include <cstddef>
#include <string>
#include <cstdio>
#include <vector>
#include <thread>
#include <fstream>

#include <wsserver.h>

class WebSocket {
public:
  enum DataType {
    DATA_PING       = READ_PING_TIME,
    DATA_TEXT       = READ_TEXT,
    DATA_BINARY     = READ_BINARY,
    DATA_FAILURE    = READ_FAILURE,
    DATA_INCOMPLETE = READ_BUFFER_OVERFLOW,
    DATA_CLOSE      = READ_CONNECTION_CLOSED
  };

  struct RawData {
    unsigned char buffer[FRAME_MAX_FRAGMENTS * FRAME_MAX_SIZE];
    size_t        size;
    DataType      type;
  };

  class ConnectionEvent {
    friend WebSocket;
  public:
    typedef void (*ConnectionCallback)(WebSocket* socket, const int client);
  public:
    void operator +=(ConnectionCallback callback);
    void operator -=(ConnectionCallback callback);
  private:
    void trigger(WebSocket* socket, const int client);
  private:
    std::vector<ConnectionCallback> callbacks;
  };
  
  class ReceptionEvent {
    friend WebSocket;
  public:
    typedef void (*ReceptionCallback)(WebSocket* socket, const int client, const RawData* data);
  public:
    void operator +=(ReceptionCallback callback);
    void operator -=(ReceptionCallback callback);
  private:
    void trigger(WebSocket* socket, const int client, const RawData* data);
  private:
    std::vector<ReceptionCallback> callbacks;
  };

public:
  WebSocket(const int port);
  ~WebSocket();

public:
  void start();
  void stop();

  void send(const int client, const void* data, const size_t size);
  void send(const int client, const char* text);
  void send(const int client, const std::string& text);

  template <typename T>
  void send(const int client, const T& serialized) {
    send(client, (void*)&serialized, sizeof(T));
  }

  int  ping(const int client);
  void disconnect(const int client);

  const std::string& message();
  const std::string& error();

private:
  void waitForMessages();
  void waitForErrors();
  void waitForConnections();
  void waitForReceptions(const int client);

  static void pong(WebSocket* socket, const int client, const RawData* data);

public:
  ConnectionEvent onConnect;
  ReceptionEvent  onReceive;
  void*           environmentPointer;

private:
  const int        port;
  std::fstream*    pingfile;
  std::FILE*       messages;
  std::FILE*       errors;
  WebSocketServer* server;
  std::thread*     serverThread;
  std::thread*     messageThread;
  std::thread*     errorThread;
  std::string      lastMessage;
  std::string      lastError;
};

#endif