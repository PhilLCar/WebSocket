#include <wsconnection.hpp>

#include <string.h>
#include <unistd.h>

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
  Connection::Connection(WebSocket* socket, WebSocketServer* server, const int client)
    : socket(socket)
    , client(client)
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
    wswrite(server, client, (unsigned char*)text, strlen(text), DATA_TEXT);
  }

  void Connection::send(const std::string& text) {
    wswrite(server, client, (unsigned char*)text.c_str(), text.length(), DATA_TEXT);
  }

  int Connection::ping() {
    long        ping_ms = -1;
    std::string name = "./ping" + std::to_string(server->port) + "_" + std::to_string(client) + ".tmp";
    pingfile = new std::fstream(name.c_str(), std::ios_base::in | std::ios_base::out);
    onReceive += pong;
    wsping(server, client);
    *pingfile >> ping_ms;
    onReceive -= pong;
    pingfile->close();
    delete pingfile;
    std::remove(name.c_str());
    return ping_ms;
  }

  bool Connection::isAlive() {
    return alive;
  }

  void Connection::listen() {
    if (!connectionThread) {
      connectionThread = new std::thread(waitForReceptions, this);
    }
  }

  void Connection::disconnect() {
    int* fd = &server->connections[client]->fd;
    if (fd >= 0) {
      close(server->connections[client]->fd);
      server->connections[client]->fd = -1;
    }
    if (connectionThread) {
      connectionThread->join();
      delete connectionThread;
      connectionThread = nullptr;
    }
  }

  void Connection::waitForReceptions() {
    RawData *data = new RawData;
    do {
      data->type = (DataType)wsread(server, client, data->buffer, FRAME_MAX_SIZE, &data->size);
      onReceive.trigger(this, data);
    } while (data->type != DATA_FAILURE && data->type != DATA_CLOSE);
    delete data;
  }



  void Connection::pong(Connection* connection, const RawData* data) {
    *connection->pingfile << (long)(void*)data->buffer;
  }
}