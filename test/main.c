#include <websocket.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct env {
  int client;
} Env;

void printm(int client, char *buffer, int type) {
  time_t     now;

  time(&now);
  {
    struct tm *local = localtime(&now);

    // In async context better not to break the print function in parts
    if (type == FRAME_TEXT) {
      printf("[%04d/%02d/%02d %02d:%02d:%02d] From client #%d: %s\n",
              local->tm_year + 1900,
              local->tm_mon + 1,
              local->tm_mday,
              local->tm_hour,
              local->tm_min,
              local->tm_sec,
              client,
              buffer);
    } else {
      printf("[%04d/%02d/%02d %02d:%02d:%02d] Ping with client #%d: %dms\n",
              local->tm_year + 1900,
              local->tm_mon + 1,
              local->tm_mday,
              local->tm_hour,
              local->tm_min,
              local->tm_sec,
              client,
              *(long*)(void*)buffer);
    }
  }
}

void receive(WebSocketServer *server, int client, unsigned char *buffer, size_t read, int status, void *environment) {
  // Make sure the test is null terminated
  buffer[read] = 0;
  printm(client, buffer, status);
}

void connect(WebSocketServer *server, int client, void *environment) {
  const char *message = "Connection established";

  ((Env*)environment)->client = client;
  printm(client, (char*)message, FRAME_TEXT);
  wswrite(server, client, message, strlen(message), FRAME_TEXT);
}

int main(int argc, char *argv[]) {
  Env env = { -1 };
  WebSocket *ws = wsalloc(8000, stdin, stderr);

  ws->env = (void*)&env;
  if (ws) {

    wsinit(ws, receive, connect);
    printf("To ping send 'p', to quit simply send an empty string.\n");
    while (1) {
      char buffer[256];
      fgets(buffer, 256, stdin);
      if (!strcmp(buffer, "")) break;
      if (env.client >= 0)
      if (!strcmp(buffer, "p")) {
        wsping(ws->server, env.client);
      } else {
        wswrite(ws->server, env.client, buffer, strlen(buffer), FRAME_TEXT);
      }
    }
    wsteardown(ws);
  }
  wsfree(ws);
  return 0;
}