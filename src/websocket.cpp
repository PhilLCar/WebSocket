/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 02 Mar 2022
 * Description: WebSocket implementation for C++.
 */

#include <websocket.hpp>

#include <algorithm>
#include <unistd.h>
#include <string.h>

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
  WebSocket::WebSocket(const int port)
    : port(port)
    , expectClose(false)
    , pingfile(nullptr)
    , messages(std::tmpfile())
    , errors(std::tmpfile())
    , server(nullptr)
    , serverThread(nullptr)
    , lastMessage("")
    , lastError("")
  {
    messageThread = new std::thread(waitForMessages, this);
    errorThread   = new std::thread(waitForErrors, this);
  }

  WebSocket::~WebSocket() {
    stop();
    delete messageThread;
    delete errorThread;
  }

  void WebSocket::start() {
    if (server) return;
    server = wsstart(port, messages, errors);
    serverThread = new std::thread(waitForConnections, this);
  }

  void WebSocket::stop() {
    if (server && server->fd >= 0) {
      // This should trigger the connection to close
      close(server->fd);
      server->fd = -1;
    }
    if (serverThread) {
      serverThread->join();
      delete serverThread;
      serverThread = nullptr;
    }
  }

  void WebSocket::sendAll(const void* data, const size_t size) {
    wsmulticast(server, (unsigned char*)data, size, DATA_BINARY);
  }

  void WebSocket::sendAll(const char* text) {
    wsmulticast(server, (unsigned char*)text, strlen(text), DATA_TEXT);
  }

  void WebSocket::sendAll(const std::string& text) {
    wsmulticast(server, (unsigned char*)text.c_str(), text.length(), DATA_TEXT);
  }

  const std::string& WebSocket::message() {
    return lastMessage;
  }

  const std::string& WebSocket::error() {
    return lastError;
  }

  void WebSocket::waitForMessages() {
    char buffer[256];

    std::fgets(buffer, sizeof(buffer), messages);
    lastMessage = std::string(buffer);
  }

  void WebSocket::waitForErrors() {
    char buffer[256];

    std::fgets(buffer, sizeof(buffer), errors);
    lastMessage = std::string(buffer);
  }

  void WebSocket::waitForConnections() {
    Connection* connections[WS_MAX_CONN] = { nullptr };

    while (true) {
      int client = wsaccept(server);
      if (client == CONNECTION_FAILURE) break;
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
        Connection* connection = new Connection(this, this->server, client);
        onConnect.trigger(connection);
        connections[client] = connection;
        connection->listen();
      }
    }
    for (int i = 0; i < WS_MAX_CONN; i++) {
      if (connections[i]) {
        delete connections[i];
        // No need to cleanup, the server is about to do it anyway
      }
    }

    wsstop(server);
    server = NULL;

    if (!expectClose) throw ServerException(this);
  }
}