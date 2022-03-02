#ifndef WEBSOCKETCONNECTION_HPP
#define WEBSOCKETCONNECTION_HPP

#include <vector>
#include <thread>
#include <fstream>
#include <algorithm>

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
    Connection(WebSocket* socket, WebSocketServer* server, const int client);
    ~Connection();

  public:
    void send(const void* data, const size_t size);
    void send(const char* text);
    void send(const std::string& text);

    template <typename T>
    inline void send(const T& serialized) {
      send((void*)&serialized, sizeof(T));
    }

    int  ping();
    bool isAlive();
    void listen();
    void disconnect();

  private:
    void waitForReceptions();
    
    static void pong(Connection *connection, const RawData* data);

  public:
    ReceptionEvent   onReceive;

  private:
    const int&       alive;
    const int        client;
    WebSocket*       socket;
    WebSocketServer* server;
    std::thread*     connectionThread;
    std::fstream*    pingfile;
  };
}

#endif