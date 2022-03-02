/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 01 Mar 2022
 * Description: WebSocket implementation for C.
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <wsserver.h>
#include <pthread.h>

typedef struct websocket {
  int              port;
  FILE            *messages;
  FILE            *errors;
  pthread_t        server_thread;
  WebSocketServer *server;
  ConnCallback     onconnect;
  ReadCallback     onread;
  void            *env;
} WebSocket;

typedef void (*ConnCallback)(WebSocketServer *server, int client, void *environment);
typedef void (*ReadCallback)(WebSocketServer *server, int client, unsigned char *buffer, size_t read, int status, void *environment);

void wsdisconnect(WebSocket *websocket, const int client);

WebSocket *wsalloc(const int port, FILE *messages, FILE *errors);
void       wsfree(WebSocket *websocket);

void wsinit(WebSocket *websocket, ConnCallback onconnect, ReadCallback onread);
void wsteardown(WebSocket *websocket);

#endif