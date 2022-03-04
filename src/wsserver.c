/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 28 Feb 2022
 * Description: WebSocket server implementation for C.
 * Standard: https://datatracker.ietf.org/doc/html/rfc6455
 */

#include <wsserver.h>
#include <http.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

const char           *SOCKET_MAGIC_STR = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const struct timeval  TIMEOUT          = { WS_TIMEOUT / 1000, (WS_TIMEOUT % 1000) * 1000};

// A conceptual frame (no specific byte format)
typedef struct frame {
  FrameHeader   header;
  unsigned char mask[WS_MASK_SIZE];
  unsigned int  length;
  unsigned char payload[FRAME_MAX_SIZE];
} Frame;

int masktoint(unsigned char *mask) {
  int imask = 0;
  for (int i = 0; i < WS_MASK_SIZE; i++) {
    imask += mask[i] << (i << 3);
  }
  return imask;
}

void inttomask(int imask, unsigned char *mask) {
  for (int i = 0; i < WS_MASK_SIZE; i++) {
    mask[i] = 0xFF & (imask >> (i << 3));
  }
}

// https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
unsigned char *encode64(const unsigned char *input, int length) {
  const int pl = 4 * ((length + 2) / 3);
  unsigned char *output = malloc((pl + 1) * sizeof(unsigned char));
  EVP_EncodeBlock(output, input, length);
  return output;
}
// https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
unsigned char *decode64(const unsigned char *input, int length) {
  const int pl = 3 * length / 4;
  unsigned char *output = malloc((pl + 1) * sizeof(unsigned char));
  EVP_DecodeBlock(output, input, length);
  return output;
}

int handshake(WebSocketConnection *connection) {
  const int bufsize = 4096;
  char buffer[bufsize];

  HttpRequest  request;
  HttpResponse response;
  memset(&request,  0, sizeof(HttpRequest));
  memset(&response, 0, sizeof(HttpResponse));
  memset(buffer, 0, bufsize * sizeof(char));
  read(connection->fd, buffer, bufsize);
  httpreqfromstr(&request, buffer);
  buffer[0] = 0;
  getfield(request.header, "Connection", buffer);
  if (!strstr(buffer, "Upgrade")) {
    free(request.body);
    return 1;
  }
  buffer[0] = 0;
  getfield(request.header, "Upgrade", buffer);
  if (!strstr(buffer, "websocket")) {
    free(request.body);
    return 1;
  }
  connection->key[0] = 0;
  getfield(request.header, "Sec-WebSocket-Key", connection->key);
  getfield(request.header, "Sec-WebSocket-Version", buffer);
  if (!connection->key[0]) {
    getfield(request.header, "Sec-Websocket-Key", connection->key);
    getfield(request.header, "Sec-Websocket-Version", buffer);
  }
  connection->version = atoi(buffer);

  // Response
  unsigned char *resp = malloc((strlen(connection->key) + strlen(SOCKET_MAGIC_STR) + 1) * sizeof(char));
  unsigned char  digest[SHA_DIGEST_LENGTH];
  unsigned char *ans;
  int   i;

  for (i = 0; connection->key[i]; i++) resp[i] = connection->key[i];
  for (int p = 0; SOCKET_MAGIC_STR[p]; i++, p++) resp[i] = SOCKET_MAGIC_STR[p];
  resp[i] = 0;
  SHA1(resp, i, digest);
  ans = encode64(digest, SHA_DIGEST_LENGTH);
  sprintf(response.version, "%s", request.version);
  response.status = HTTP_SWITCH;
  sprintf(response.message, HTTP_SWITCH_M);
  buildhttpresp(&response, "Upgrade", "websocket");
  buildhttpresp(&response, "Connection", "Upgrade");
  buildhttpresp(&response, "Sec-WebSocket-Accept", (char*)ans);
  response.body = "";
  httprespstr(&response, buffer);
  write(connection->fd, buffer, strlen(buffer));

  free(resp);
  free(ans);
  free(request.body);
  return 0;
}

void wsmulticast(WebSocketServer *server, const void *buffer, const size_t size, const int type) {
  for (int i = 0; i < WS_MAX_CONN; i++) {
    if (server->connections[i]) wswrite(server, i, buffer, size, type);
  }
}

void wsping(WebSocketServer *server, int client) {
  wswrite(server, client, NULL, 0, FRAME_PING);
}

void wswrite(WebSocketServer *server, const int client, const unsigned char *buffer, const size_t size, const int type) {
  unsigned char        mask[WS_MASK_SIZE];
  WebSocketConnection *connection = server->connections[client];
  int                  masked     = WS_MASK != 0;

  memset(&mask, 0, WS_MASK_SIZE * sizeof(unsigned char));
  if (masked) inttomask(WS_MASK, mask);

  // If size is 0, it's a ping
  if (!size) {
    ControlFrame  cframe;
    int  i = 0;

    memset(&cframe.header, 0, sizeof(FrameHeader));
    cframe.header.end    = 1;
    cframe.header.mask   = masked;
    cframe.header.length = size;
    cframe.header.opcode = FRAME_PING;
    cframe.header.bytes  = htons(cframe.header.bytes);
    if (masked) {
      for (i = 0; i < WS_MASK_SIZE; i++) {
        cframe.spayload[i] = mask[i];
      }
    }
    write(connection->fd, &cframe, size + sizeof(FrameHeader) + (masked ? WS_MASK_SIZE : 0));
    connection->ping = clock();
  } 
  // If it fits in a Control Frame
  else if (size <= FRAME_CONTROL_SIZE) {
    ControlFrame  cframe;

    memset(&cframe.header, 0, sizeof(FrameHeader));
    cframe.header.end    = 1;
    cframe.header.mask   = masked;
    cframe.header.length = size;
    cframe.header.opcode = type;
    cframe.header.bytes  = htons(cframe.header.bytes);
    if (masked) {
      for (int i = 0; i < WS_MASK_SIZE; i++) cframe.spayload[i] = mask[i];
      for (int i = 0; i < size;         i++) cframe.payload[i]  = buffer[i] ^ mask[i % WS_MASK_SIZE];
      write(connection->fd, &cframe, size + sizeof(FrameHeader) + WS_MASK_SIZE* sizeof(unsigned char));
    } else {
      for (int i = 0; i < size; i++) cframe.spayload[i] = buffer[i];
      write(connection->fd, &cframe, size + sizeof(FrameHeader));
    }
  }
  // If it fits in a Long Frame
  else if (size <= FRAME_MAX_SIZE) {
    LongFrame lframe;

    memset(&lframe.header, 0, sizeof(FrameHeader));
    lframe.header.end    = 1;
    lframe.header.mask   = masked;
    lframe.header.length = 126;
    lframe.header.opcode = type;
    lframe.header.bytes  = htons(lframe.header.bytes);
    lframe.length        = htons((unsigned short)size);
    if (masked) {
      for (int i = 0; i < WS_MASK_SIZE; i++) lframe.spayload[i] = mask[i];
      for (int i = 0; i < size;         i++) lframe.payload[i]  = buffer[i] ^ mask[i % WS_MASK_SIZE];
      write(connection->fd, &lframe, size + sizeof(FrameHeader) +
                                            sizeof(unsigned short) +
                                            WS_MASK_SIZE * sizeof(unsigned char));
    } else {
      for (int i = 0; i < size; i++) lframe.spayload[i] = buffer[i];
      write(connection->fd, &lframe, size + sizeof(FrameHeader) + sizeof(unsigned short));
    }
  } else {
    int frames  = size / FRAME_MAX_SIZE;
    for (int j = 0; j < frames; j++) {
      LongFrame lframe;
      int last  = j == (frames - 1);
      int fsize = last ? size % FRAME_MAX_SIZE : FRAME_MAX_SIZE;

      memset(&lframe.header, 0, sizeof(FrameHeader));
      lframe.header.end    = last;
      lframe.header.mask   = masked;
      lframe.header.length = 126;
      lframe.header.opcode = type;
      lframe.header.bytes  = htons(lframe.header.bytes);
      lframe.length        = htons((unsigned short)size);
      if (masked && !j) {
        for (int i = 0; i < WS_MASK_SIZE;   i++) lframe.spayload[i] = mask[i];
        for (int i = 0; i < FRAME_MAX_SIZE; i++) lframe.payload[i]  = buffer[i] ^ mask[i % WS_MASK_SIZE];
        write(connection->fd, &lframe, FRAME_MAX_SIZE * sizeof(unsigned char) + 
                                       sizeof(FrameHeader) + 
                                       sizeof(unsigned short) + 
                                       WS_MASK_SIZE * sizeof(unsigned char));
      } else {
        int done = j * FRAME_MAX_SIZE;
        for (int i = 0; i < FRAME_MAX_SIZE && done + i < size; i++) {
          lframe.spayload[i] = buffer[done + i] ^ mask[(done + i) % WS_MASK_SIZE];
        }
        write(connection->fd, &lframe, (last ? fsize : FRAME_MAX_SIZE) * sizeof(unsigned char) + 
                                      sizeof(FrameHeader) + 
                                      sizeof(unsigned short));
      }
    }
  }
}

int wsread(WebSocketServer *server, const int client, unsigned char *buffer, const size_t maxbytes, size_t *readbytes) {
  Frame                frame;
  ControlFrame         cframe;
  WebSocketConnection *connection = server->connections[client];
  int                  expect     = 0;
  int                  readstatus = READ_FAILURE;
  struct timeval       timeout    = TIMEOUT;

  *readbytes = 0;

  while (connection->active) {
    fd_set input;
    FD_ZERO(&input);
    FD_SET(connection->fd, &input);
    int n = select(connection->fd + 1, &input, NULL, NULL, &timeout);
    if (n < 0) {
      connection->active = 0;
      fprintf(server->messages, "Connection was closed by server\n");
      return READ_CONNECTION_CLOSED_SERVER;
    }
    else if (n == 0) continue; // select timed out

    // Frame header
    {
      short header;
      int   bytes = read(connection->fd, &header, sizeof(short));
      if (!bytes) {
        connection->active = 0;
        fprintf(server->errors, "Connection was closed by client unexpectedly\n");
        return READ_CONNECTION_CLOSED_CLIENT;
      }
      frame.header.bytes = ntohs(header);
    }
    if (frame.header.length < 126) {
      frame.length = frame.header.length;
    } else if (frame.header.length == 126) {
      short l;
      read(connection->fd, &l, sizeof(short));
      frame.length = ntohs(l);
    } else if (frame.header.length == 127) {
      // Unsupported: Long Long Frame
      int l;
      read(connection->fd, &l, sizeof(int));
      frame.length = ntohl(l);
    }
    if (frame.header.mask) {
      read(connection->fd, frame.mask, WS_MASK_SIZE);
    } else {
      memset(frame.mask, 0, WS_MASK_SIZE * sizeof(unsigned char));
    }
    read(connection->fd, frame.payload, frame.length);

    switch (frame.header.opcode) {
      case FRAME_CONTINUE:
      case FRAME_TEXT:
      case FRAME_BINARY:
        if (frame.header.opcode == FRAME_CONTINUE && !expect) {
          fprintf(server->errors, "Received a continued frame without previous opcode!\n");
          // TODO: EMPTY STREAM
          break;
        } else if (frame.header.opcode == FRAME_BINARY) {
          readstatus = READ_BINARY;
        } else if (frame.header.opcode == FRAME_TEXT) {
          readstatus = READ_TEXT;
        }
        expect = !frame.header.end;
        {
          int i;
          for (i = *readbytes; i < frame.length && i < maxbytes; i++) {
            buffer[i] = frame.payload[i] ^ frame.mask[i % WS_MASK_SIZE];
          }
          *readbytes = i;
          if (i == maxbytes) return READ_BUFFER_OVERFLOW;
        }
        if (!expect) return readstatus;
        break;
      case FRAME_CLOSE:
        memset(&cframe, 0, sizeof(ControlFrame));
        cframe.header.opcode = FRAME_CLOSE;
        cframe.header.end    = 1;
        cframe.header.mask   = WS_MASK != 0;
        cframe.header.length = 0;
        if (WS_MASK) {
          inttomask(WS_MASK, cframe.mask);
          write(connection->fd, &cframe, sizeof(FrameHeader) + WS_MASK_SIZE * sizeof(unsigned char));
        } else {
          write(connection->fd, &cframe, sizeof(FrameHeader));
        }
        connection->active = 0;
        fprintf(server->messages, "Connection was closed by client\n");
        shutdown(connection->fd, SHUT_RDWR);
        return READ_CONNECTION_CLOSED_CLIENT;
      case FRAME_PING:
        memset(&cframe, 0, sizeof(ControlFrame));
        cframe.header.opcode = FRAME_PONG;
        cframe.header.end    = 1;
        cframe.header.mask   = frame.header.mask;
        cframe.header.length = 0;
        if (WS_MASK) {
          inttomask(WS_MASK, cframe.mask);
          write(connection->fd, &cframe, sizeof(FrameHeader) + WS_MASK_SIZE * sizeof(unsigned char));
        } else {
          write(connection->fd, &cframe, sizeof(FrameHeader));
        }
        break;
      case FRAME_PONG:
        *(long*)(void*)buffer = (long)(clock() - connection->ping) / (CLOCKS_PER_SEC / 1000);
        *readbytes = sizeof(long);
        return READ_PING_TIME;
      default:
        fprintf(server->errors, "Fatal error: unimplemented (wsread)\n");
        return READ_FAILURE;
    }
  }
  return readstatus;
}

int wsaccept(WebSocketServer *server) {
  int       client = CONNECTION_MAX_READCHED;
  int       client_fd;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  if ((client_fd = accept(server->fd, (struct sockaddr *restrict)&server->address, &addrlen)) < 0) {
    for (int i = 0; i < WS_MAX_CONN; i++) wsclose(server, i);
    if (server->close) {
      fprintf(server->messages, "Closing server\n");
      return CONNECTION_CLOSED;
    }
    fprintf(server->errors, "Cannot accept\n");
    return CONNECTION_FAILURE;
  }

  for (int i = 0; i < WS_MAX_CONN; i++) {
    if (!server->connections[i]) {
      WebSocketConnection *connection = malloc(sizeof(WebSocketConnection));

      memset(connection, 0, sizeof(WebSocketConnection));
      connection->active = 1;
      connection->fd     = client_fd;

      if (handshake(connection)) {
        client = CONNECTION_BAD_HANDSHAKE;
        free(connection);
      } else {
        client = i;
        server->connections[i] = connection;
      }
      break;
    }
  }
  if (client >= 0) {
    fprintf(server->messages, "Connection with client %d success\n", client);
  } else if (client == CONNECTION_BAD_HANDSHAKE) {
    fprintf(server->errors, "Failed to perform handshake\n");
    close(client_fd);
  } else {
    fprintf(server->errors, "Max connections reached\n");
    close(client_fd);
  }
  return client;
}

void wsclose(WebSocketServer *server, int client) {
  WebSocketConnection *connection = server->connections[client];

  if (connection) {
    shutdown(connection->fd, SHUT_RDWR);
    close(connection->fd);
    free(connection);
    server->connections[client] = NULL;
  }
}

WebSocketServer *wsstart(const short port, FILE *messages, FILE *errors) {
  WebSocketServer *server = malloc(sizeof(WebSocketServer));
  if (server) {
    int                 server_fd;
    struct sockaddr_in *address;

    server->port     = port;
    server->messages = messages;
    server->errors   = errors;
    address          = &server->address;
    memset(server->connections, 0, WS_MAX_CONN * sizeof(WebSocketConnection*));

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
      fprintf(errors, "Cannot create socket\n");
      return NULL;
    }
    fprintf(messages, "WebSocket Server created successfully\n");
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
      fprintf(errors, "Cannot reuse socket\n");
      return NULL;
    }
    fprintf(messages, "Socket setup successful\n");

    memset(address, 0, sizeof(struct sockaddr_in));
    memset(address->sin_zero, 0, sizeof(unsigned char));
    address->sin_family      = AF_INET;
    address->sin_addr.s_addr = htonl(INADDR_ANY);
    address->sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *restrict)address, sizeof(struct sockaddr_in)) < 0) {
      fprintf(errors, "Bind failed\n");
      return NULL;
    }
    fprintf(messages, "Socket binded successfuly\n");

    if (listen(server_fd, WS_MAX_CONN) < 0) {
      fprintf(errors, "Cannot listen\n");
      return NULL;
    }
    server->fd = server_fd;
    fprintf(messages, "Listening on port %d for WebSocket connections...\n", port);
  }
  return server;
}

void wsstop(WebSocketServer *server) {
  if (server) {
    for (int i = 0; i < WS_MAX_CONN; i++) wsclose(server, i);
    shutdown(server->fd, SHUT_RDWR);
    close(server->fd);
    free(server);
  }
}