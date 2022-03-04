/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 03 Mar 2022
 * Description: WebSocket connection implementation for C++.
 */

#ifndef WEBSOCKETCONNECTION_HPP
#define WEBSOCKETCONNECTION_HPP

#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>

#include <wstypes.hpp>

namespace ws {
  class WebSocket;
  class Connection {
  public:
    class ReceptionEvent {
      friend Connection;
    public:
      typedef void (*ReceptionCallback)(Connection* connection, const RawData* data);
    public:
      void operator +=(ReceptionCallback callback);
      void operator -=(ReceptionCallback callback);
    private:
      void trigger(Connection* connection, const RawData* data);
    private:
      std::vector<ReceptionCallback> callbacks;
    };
  public:
    Connection(WebSocketServer* server, const int client, const void* envPtr);
    ~Connection();

  public:
    void send(const void* data, const size_t size);
    void send(const char* text);
    void send(const std::string& text);

    template <typename T>
    inline void send(const T& serialized) {
      send((void*)&serialized, sizeof(T));
    }

    int  ping(int timeout_ms = 1000);
    bool isAlive();
    void listen();
    void disconnect();

    const int   getClientID();

    template <typename T>
    inline T* getEnvPtr() {
      return (T*)envPtr;
    }

  private:
    void waitForReceptions();
    
    static void pong(Connection *connection, const RawData* data);

  public:
    ReceptionEvent   onReceive;

  private:
    WebSocketServer* server;
    const int        client;
    const void*      envPtr;
    const int&       alive;
    std::thread*     connectionThread;
    std::timed_mutex pingMutex;
    long             ping_ms;
  };
}

#endif