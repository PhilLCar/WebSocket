/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 03 Mar 2022
 * Description: Very simple demo for the websocket.h header for C. (Use with test.html)
 */

#include <websocket.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

// Put your global variables in such a struct
typedef struct env {
  int client;
} Env;

void printm(int client, char *buffer, int type) {
  time_t now;

  time(&now);
  {
    struct tm *local = localtime(&now);

    // In async context better not to break the print function in parts
    if (type == READ_TEXT) {
      printf("[%04d/%02d/%02d %02d:%02d:%02d] From client #%d: %s\n",
              local->tm_year + 1900,
              local->tm_mon + 1,
              local->tm_mday,
              local->tm_hour,
              local->tm_min,
              local->tm_sec,
              client,
              buffer);
    } else if (type == READ_PING_TIME) {
      printf("[%04d/%02d/%02d %02d:%02d:%02d] Ping with client #%d: %dms\n",
              local->tm_year + 1900,
              local->tm_mon + 1,
              local->tm_mday,
              local->tm_hour,
              local->tm_min,
              local->tm_sec,
              client,
              (int)*(long*)(void*)buffer);
    }
  }
}

void reception(WebSocketServer *server, int client, unsigned char *buffer, size_t read, int status, void *environment) {
  if (status != READ_TEXT && status != READ_PING_TIME) {
    // Disconnect
    ((Env*)environment)->client = -1;
  } else {
    // Make sure the buffer is null terminated
    buffer[read] = 0;
    printm(client, (char*)buffer, status);
  }
}

void connection(WebSocketServer *server, int client, void *environment) {
  const char *message = "Connection established";

  ((Env*)environment)->client = client;
  printm(client, (char*)message, FRAME_TEXT);
  wswrite(server, client, (const unsigned char*)message, strlen(message), FRAME_TEXT);
}

int main(int argc, char *argv[]) {
  Env env = { -1 };
  WebSocket *ws = wsalloc(8000, stdout, stderr);

  ws->env = (void*)&env;
  if (ws) {

    wsinit(ws, connection, reception);
    printf("To ping send 'p', to quit simply send an empty string.\n");
    while (1) {
      char buffer[256];
      fgets(buffer, 256, stdin);
      if (!strcmp(buffer, "\n")) break;
      if (env.client >= 0) {
        if (!strcmp(buffer, "p\n")) {
          wsping(ws->server, env.client);
        } else {
          wswrite(ws->server, env.client, (const unsigned char*)buffer, strlen(buffer), FRAME_TEXT);
        }
      }
    }
    wsteardown(ws);
  }
  wsfree(ws);
  return 0;
}