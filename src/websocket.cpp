/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 02 Mar 2022
 * Description: WebSocket implementation for C++.
 */

#include <websocket.hpp>

#include <algorithm>
#include <cstring>
#include <fstream>

namespace ws {
  // ServerException
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  WebSocket::ServerException::ServerException(WebSocket* socket)
    : socket(socket)
  {
    exception = "Unknown server error (port " + std::to_string(socket->port) + ")";
  }

  const char *WebSocket::ServerException::what() {
    return exception.c_str();
  }

  // ConnectionEvent
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  void WebSocket::ConnectionEvent::trigger(Connection* connection) {
    for (auto callback : callbacks) callback(connection);
  }

  void WebSocket::ConnectionEvent::operator +=(ConnectionCallback callback) {
    callbacks.push_back(callback);
  }

  void WebSocket::ConnectionEvent::operator -=(ConnectionCallback callback) {
    callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback), callbacks.end());
  }

  // WebSocket
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  WebSocket::WebSocket(const int port, const void* envPtr)
    : port(port)
    , envPtr(envPtr)
    , server(nullptr)
    , serverThread(nullptr)
    , lastMessage("")
    , lastError("")
    , mname(".messages." + std::to_string(port) + ".tmp")
    , ename(".errors."   + std::to_string(port) + ".tmp")
  {
    messages = std::fopen(mname.c_str(), "w+");
    errors   = std::fopen(ename.c_str(), "w+");
  }

  WebSocket::WebSocket(const int port) : WebSocket(port, nullptr) {}

  WebSocket::~WebSocket() {
    stop();

    std::remove(mname.c_str());
    std::remove(ename.c_str());
  }

  void WebSocket::start() {
    if (server) return;
    server = wsstart(port, messages, errors);
    serverThread = new std::thread(&ws::WebSocket::waitForConnections, this);
  }

  void WebSocket::stop() {
    if (server) {
      wsstop(server);
      if (serverThread) {
        serverThread->join();
        delete serverThread;
        serverThread = nullptr;
      }
      server = NULL;
    }
  }

  void WebSocket::sendAll(const void* data, const size_t size) {
    wsmulticast(server, (unsigned char*)data, size, DATA_BINARY);
  }

  void WebSocket::sendAll(const char* text) {
    wsmulticast(server, (unsigned char*)text, std::strlen(text), DATA_TEXT);
  }

  void WebSocket::sendAll(const std::string& text) {
    wsmulticast(server, (unsigned char*)text.c_str(), text.length(), DATA_TEXT);
  }

  const std::string& WebSocket::message() {
    std::fflush(messages);
    {
      std::ifstream mfile(mname, std::ios_base::in);
      std::string   m;

      while (std::getline(mfile, m)) lastMessage = m;
      mfile.close();
    }
    return lastMessage;
  }

  const std::string& WebSocket::error() {
    std::fflush(errors);
    {
      std::ifstream mfile(mname, std::ios_base::in);
      std::string   m;

      while (std::getline(mfile, lastMessage)) lastError = m;
      mfile.close();
    }
    return lastMessage;
  }

  void WebSocket::waitForConnections() {
    int         client;
    Connection* connections[WS_MAX_CONN] = { nullptr };

    while (true) {
      client = wsaccept(server);
      if (client == CONNECTION_FAILURE || client == CONNECTION_CLOSED) break;
      // Purge the old connections
      for (int i = 0; i < WS_MAX_CONN; i++) {
        // This means that the thread is still going but that the connection is closed
        if (connections[i] && !connections[i]->isAlive()) {
          delete connections[i];
          connections[i] = nullptr;
          wsclose(server, i);
        }
      }
      if (client != CONNECTION_BAD_HANDSHAKE && client != CONNECTION_MAX_READCHED) {
        Connection* connection = new Connection(this->server, client, envPtr);
        onConnect.trigger(connection);
        connections[client] = connection;
        connection->listen();
      }
    }
    for (int i = 0; i < WS_MAX_CONN; i++) {
      if (connections[i]) {
        connections[i]->disconnect();
        delete connections[i];
      }
    }

    if (client != CONNECTION_CLOSED) throw ServerException(this);
  }
}