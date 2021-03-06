/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 01 Mar 2022
 * Description: WebSocket implementation for C.
 */

#include <websocket.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void *wslisten(void *vargp) {
  int            readstatus;
  size_t         readbytes  = 0;
  unsigned char *buffer     = malloc(FRAME_MAX_FRAGMENTS * FRAME_MAX_SIZE * sizeof(unsigned char));
  WebSocket     *websocket  = ((WebSocket**)vargp)[0];
  int            client     =       ((long*)vargp)[1];
  do {
    readstatus = wsread(websocket->server, client, buffer, FRAME_MAX_SIZE, &readbytes);
    websocket->onread(websocket->server, client, buffer, readbytes, readstatus, websocket->env);
  } while (readstatus >= 0 || readstatus == READ_BUFFER_OVERFLOW); // (Buffer overflow is not a fatal error)
  
  free(buffer);
  return NULL;
}


void *wsconnect(void *vargp) {
  pthread_t           client_thread[WS_MAX_CONN];
  WebSocket          *websocket = (WebSocket*)vargp;

  memset(client_thread, 0, WS_MAX_CONN * sizeof(pthread_t));
  while (1) {
    int client = wsaccept(websocket->server);
    if (client == CONNECTION_FAILURE || client == CONNECTION_CLOSED) break;
    // Purge the old connections
    for (int i = 0; i < WS_MAX_CONN; i++) {
      // This means that the thread is still going but that the connection is closed
      if (client_thread[i] && !websocket->server->connections[i]->active) {
        pthread_join(client_thread[i], NULL);
        client_thread[i] = 0;
        wsclose(websocket->server, i);
      }
    }
    if (client != CONNECTION_BAD_HANDSHAKE && client != CONNECTION_MAX_READCHED) {
      void **vargp = malloc(2 * sizeof(void*));
      if (vargp) {
        vargp[0] = (void*)websocket;
        vargp[1] = (void*)(long)client;
        websocket->onconnect(websocket->server, client, websocket->env);
        pthread_create(&client_thread[client], NULL, wslisten, vargp);
      } else {
        wsclose(websocket->server, client);
      }
    }
  }
  for (int i = 0; i < WS_MAX_CONN; i++) {
    if (client_thread[i]) {
      wsclose(websocket->server, i);
      pthread_join(client_thread[i], NULL);
    }
  }

  return NULL;
}

WebSocket *wsalloc(const int port, FILE *messages, FILE *errors) {
  WebSocket *websocket = malloc(sizeof(WebSocket));
  
  if (websocket) {
    websocket->port     = port;
    websocket->messages = messages;
    websocket->errors   = errors;
  }

  return websocket;
}

void wsfree(WebSocket *websocket) {
  wsteardown(websocket);
  free(websocket);
}

void wsinit(WebSocket *websocket, ConnCallback onconnect, ReadCallback onread) {
  if (websocket->server) return;
  websocket->server    = wsstart(websocket->port, websocket->messages, websocket->errors);
  websocket->onconnect = onconnect;
  websocket->onread    = onread;
  pthread_create(&websocket->server_thread, NULL, wsconnect, (void*)websocket);
}

void wsteardown(WebSocket *websocket) {
  if (websocket->server) {
    wsstop(websocket->server);
    if (websocket->server_thread) {
      pthread_join(websocket->server_thread, NULL);
      websocket->server_thread = 0;
    }
    websocket->server = NULL;
  }
}