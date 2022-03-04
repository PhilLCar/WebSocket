/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 02 Mar 2022
 * Description: WebSocket implementation for C++.
 */

#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP

#include <string>
#include <vector>
#include <thread>
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

    template <typename T>
    inline WebSocket(const int port, const T& env) : WebSocket(port, (const void*)&env) {}

  private:
    WebSocket(const int port, const void* envPtr);

  public:
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
    void waitForConnections();

  public:
    ConnectionEvent onConnect;

  private:
    const int        port;
    const void*      envPtr;
    std::FILE*       messages;
    std::FILE*       errors;
    WebSocketServer* server;
    std::thread*     serverThread;
    std::string      lastMessage;
    std::string      lastError;
    std::string      mname;
    std::string      ename;
  };
}

#endif