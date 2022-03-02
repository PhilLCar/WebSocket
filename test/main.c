#include <websocket.h>

#include <stdio.h>

typedef struct env {

} Env;

void receive(WebSocketServer *server, int client, unsigned char *buffer, size_t read, int status, void *environment) {

}

void connect(WebSocketServer *server, int client, void *environment) {
  const char *connection = "Connected, woohoo!"
}

int main(int argc, char *argv[]) {
  Env env;
  WebSocket *ws = wsalloc(8000, stdin, stderr);
  if (ws) {
    ws->env = (void*)&env;
    wsinit(ws, receive, connect);

    system("pause");

    wsteardown(ws);
  }
  wsfree(ws);
  return 0;
}