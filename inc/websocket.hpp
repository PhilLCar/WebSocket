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
#include <exception>

#include <wsconnection.hpp>
#include <wsserver.h>

namespace ws {
  class WebSocket {
  public:
    class ServerException : std::exception {
    public:
      ServerException(WebSocket* socket);
      const char *what();
    private:
      std::string exception;
      WebSocket*  socket;
    };

    class ConnectionEvent {
      friend WebSocket;
    public:
      typedef void (*ConnectionCallback)(Connection* connection);
    public:
      void operator +=(ConnectionCallback callback);
      void operator -=(ConnectionCallback callback);
    private:
      void trigger(Connection* connection);
    private:
      std::vector<ConnectionCallback> callbacks;
    };

  public:
    WebSocket(const int port);
    ~WebSocket();

  public:
    void start();
    void stop();

    void sendAll(const void* data, const size_t size);
    void sendAll(const char* text);
    void sendAll(const std::string& text);

    template <typename T>
    inline void sendAll(const T& serialized) {
      sendAll((void*)&serialized, sizeof(T));
    }

    const std::string& message();
    const std::string& error();

  private:
    void waitForMessages();
    void waitForErrors();
    void waitForConnections();

  public:
    ConnectionEvent onConnect;
    void*           environmentPointer;

  private:
    const int                         port;
    bool                              expectClose;
    std::fstream*                     pingfile;
    std::FILE*                        messages;
    std::FILE*                        errors;
    WebSocketServer*                  server;
    std::thread*                      serverThread;
    std::thread*                      messageThread;
    std::thread*                      errorThread;
    std::string                       lastMessage;
    std::string                       lastError;
    std::vector<WebSocketConnection*> connections;
  };
}

#endif