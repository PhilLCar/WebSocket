/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 03 Mar 2022
 * Description: WebSocket connection implementation for C++.
 */

#include <wsconnection.hpp>

#include <chrono>
#include <cstring>

namespace ws {
  // ReceptionEvent
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  void Connection::ReceptionEvent::trigger(Connection* connection, const RawData *data) {
    for (auto callback : callbacks) callback(connection, data);
  }

  void Connection::ReceptionEvent::operator +=(ReceptionCallback callback) {
    callbacks.push_back(callback);
  }

  void Connection::ReceptionEvent::operator -=(ReceptionCallback callback) {
    callbacks.erase(std::remove(callbacks.begin(), callbacks.end(), callback), callbacks.end());
  }

  // WebSocketConnection
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  Connection::Connection(WebSocketServer* server, const int client, const void* envPtr)
    : server(server)
    , client(client)
    , envPtr(envPtr)
    , alive(server->connections[client]->active)
    , connectionThread(nullptr)
  {
  }

  Connection::~Connection() {
    disconnect();
  }

  void Connection::send(const void* data, const size_t size) {
    wswrite(server, client, (unsigned char*)data, size, DATA_BINARY);
  }

  void Connection::send(const char* text) {
    wswrite(server, client, (unsigned char*)text, std::strlen(text), DATA_TEXT);
  }

  void Connection::send(const std::string& text) {
    wswrite(server, client, (unsigned char*)text.c_str(), text.length(), DATA_TEXT);
  }

  int Connection::ping(int timeout_ms) {
    int p = -1;
    pingMutex.lock();
    onReceive += pong;
    wsping(server, client);
    if (pingMutex.try_lock_for(std::chrono::milliseconds(timeout_ms))) {
      // Timeout
      p = ping_ms;
    }
    onReceive -= pong;
    pingMutex.unlock();
    return p;
  }

  bool Connection::isAlive() {
    return alive;
  }

  void Connection::listen() {
    if (!connectionThread) {
      connectionThread = new std::thread(&ws::Connection::waitForReceptions, this);
    }
  }

  void Connection::disconnect() {
    if (connectionThread) {
      wsclose(server, client);
      if (connectionThread) {
        connectionThread->join();
        delete connectionThread;
        connectionThread = nullptr;
      }
    }
  }

  const int Connection::getClientID() {
    return client;
  }

  void Connection::waitForReceptions() {
    RawData *data = new RawData;
    do {
      data->type = (DataType)wsread(server, client, data->buffer, FRAME_MAX_SIZE, &data->size);
      onReceive.trigger(this, data);
    } while (data->type >= 0 || data->type == DATA_INCOMPLETE);
    delete data;
  }



  void Connection::pong(Connection* connection, const RawData* data) {
    if (data->type == DATA_PING) {
      connection->ping_ms = *(long*)(void*)data->buffer;
      connection->pingMutex.unlock();
    }
  }
}