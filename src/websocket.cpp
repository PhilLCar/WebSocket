/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 02 Mar 2022
 * Description: WebSocket implementation for C++.
 */

#include <websocket.hpp>

#include <algorithm>
#include <unistd.h>
#include <string.h>

// ConnectionEvent
///////////////////////////////////////////////////////////////////////////////////////////////////////
void WebSocket::ConnectionEvent::trigger(WebSocket* socket, const int client) {
  for (auto callback : callbacks) callback(socket, client);
}

void WebSocket::ConnectionEvent::operator +=(ConnectionCallback callback) {
  callbacks.push_back(callback);
}

void WebSocket::ConnectionEvent::operator -=(ConnectionCallback callback) {
  callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback), callbacks.end());
}

// ReceptionEvent
///////////////////////////////////////////////////////////////////////////////////////////////////////
void WebSocket::ReceptionEvent::trigger(WebSocket* socket, const int client, const RawData *data) {
  for (auto callback : callbacks) callback(socket, client, data);
}

void WebSocket::ReceptionEvent::operator +=(ReceptionCallback callback) {
  callbacks.push_back(callback);
}

void WebSocket::ReceptionEvent::operator -=(ReceptionCallback callback) {
  callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback), callbacks.end());
}

// WebSocket
///////////////////////////////////////////////////////////////////////////////////////////////////////
WebSocket::WebSocket(const int port)
  : port(port)
  , messages(std::tmpfile())
  , errors(std::tmpfile())
  , server(nullptr)
  , serverThread(nullptr)
{
  messageThread = new std::thread(waitForMessages, this);
  errorThread   = new std::thread(waitForErrors, this);
}

WebSocket::~WebSocket() {
  delete messageThread;
  delete errorThread;
}

void WebSocket::start() {
  if (server) return;
  server = wsstart(port, messages, errors);
  serverThread = new std::thread(waitForConnections, this);
}

void WebSocket::stop() {
  if (server) {
    // This should trigger the connection to close
    close(server->fd);
  }
  if (serverThread) {
    serverThread->join();
    delete serverThread;
    serverThread = nullptr;
  }
}

void WebSocket::send(const int client, const void* data, const size_t size) {
  if (client < 0) { // multicast
    wsmulticast(server, (unsigned char*)data, size, DATA_BINARY);
  } else {
    wswrite(server, client, (unsigned char*)data, size, DATA_BINARY);
  }
}

void WebSocket::send(const int client, const char* text) {
  if (client < 0) { // multicast
    wsmulticast(server, (unsigned char*)text, strlen(text), DATA_TEXT);
  } else {
    wswrite(server, client, (unsigned char*)text, strlen(text), DATA_TEXT);
  }
}

void WebSocket::send(const int client, const std::string& text) {
  if (client < 0) { // multicast
    wsmulticast(server, (unsigned char*)text.c_str(), text.length(), DATA_TEXT);
  } else {
    wswrite(server, client, (unsigned char*)text.c_str(), text.length(), DATA_TEXT);
  }
}

void WebSocket::ping(const int client) {

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
  std::thread *clientThread[WS_MAX_CONN] = { nullptr };

  while (true) {
    int client = wsaccept(server);
    if (client == CONNECTION_FAILURE) break;
    // Purge the old connections
    for (int i = 0; i < WS_MAX_CONN; i++) {
      // This means that the thread is still going but that the connection is closed
      if (clientThread[i] && !server->connections[i]->active) {
        clientThread[i]->join();
        delete clientThread[i];
        clientThread[i] = nullptr;
        wsclose(server, i);
      }
    }
    if (client != CONNECTION_BAD_HANDSHAKE && client != CONNECTION_MAX_READCHED) {
      onConnection.trigger(this, client);
      clientThread[client] = new std::thread(waitForMessages, this, client);
    }
  }
  for (int i = 0; i < WS_MAX_CONN; i++) {
    if (clientThread[i]) {
      clientThread[i]->join();
      delete clientThread[i];
      // No need to cleanup, the server is about to do it anyway
    }
  }

  wsstop(server);
  server = NULL;
}

void WebSocket::waitForReceptions(const int client) {
  RawData *data = new RawData;
  do {
    data->type = (DataType)wsread(server, client, data->buffer, FRAME_MAX_SIZE, &data->size);
    onReception.trigger(this, client, data);
  } while (data->type != DATA_FAILURE && data->type != DATA_CLOSE);
  delete data;
}