/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 28 Feb 2022
 * Description: WebSocket server implementation for C.
 * Standard: https://datatracker.ietf.org/doc/html/rfc6455
 */

#ifndef WSSERVER_H
#define WSSERVER_H

#include <time.h>
#include <stdio.h>
#include <netinet/in.h>

/*
NOTE: 
This is designed for a Little Endian system (Intel), it hasn't been tested on a Big Endian system.
*/

/*
NOTE:
The mask thing seems dumb. Since the mask is sent in the clear along with the message, anyone with
the skill to listen will have the skill to apply a one-time pad. As it is, it's just a waste of
processing, hence why it's left at 0.
*/
#define WS_MAX_CONN          32
#define WS_KEY_SIZE          64
#define WS_TIMEOUT         3000
#define WS_MASK      0x00000000
#define WS_MASK_SIZE          4

/*
NOTE:
If we wanted to transmit a lot of data (bigger than 4 Long Frames), this implementation wouldn't work
(READ_BUFFER_OVERFLOW). It would be possible to change it, but (1) it would come at a certain
performance cost and (2) there are already a lot of protocols in place for file transfer or streaming.
The point of WebSockets is a quick "instantaneous" and interractive connection to the server. In this
regard, this implementation is not concerned with large data transfers.
*/
#define FRAME_MAX_FRAGMENTS    4
#define FRAME_MAX_SIZE     65536
#define FRAME_CONTROL_SIZE   125
#define FRAME_CONTINUE       0x0
#define FRAME_TEXT           0x1
#define FRAME_BINARY         0x2
#define FRAME_CLOSE          0x8
#define FRAME_PING           0x9
#define FRAME_PONG           0xA

#define READ_PING_TIME           FRAME_PING
#define READ_TEXT                FRAME_TEXT
#define READ_BINARY            FRAME_BINARY
#define READ_FAILURE                     -1
#define READ_BUFFER_OVERFLOW             -2
#define READ_CONNECTION_CLOSED           -3

#define CONNECTION_FAILURE       -1
#define CONNECTION_MAX_READCHED  -2
#define CONNECTION_BAD_HANDSHAKE -3

typedef struct websocket_connection {
  int                   active;
  int                   fd;
  clock_t               ping;
  char                  key[WS_KEY_SIZE];
  int                   version;
} WebSocketConnection;

typedef struct websocket_server {
  short                port;
  int                  fd;
  struct sockaddr_in   address;
  FILE                *messages;
  FILE                *errors;
  WebSocketConnection *connections[WS_MAX_CONN];
} WebSocketServer;

#pragma pack(push, 1)
typedef struct frame_header {
  union {
    struct {
      unsigned int  end    : 1;
      unsigned int  rsv1   : 1;
      unsigned int  rsv2   : 1;
      unsigned int  rsv3   : 1;
      unsigned int  opcode : 4;
      unsigned int  mask   : 1;
      unsigned int  length : 7;
    };
    unsigned short bytes;
  };
} FrameHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct control_frame {
  FrameHeader header;
  union {
    unsigned char spayload[FRAME_CONTROL_SIZE + WS_MASK_SIZE];
    struct {
      unsigned char mask[WS_MASK_SIZE];
      unsigned char payload[FRAME_CONTROL_SIZE];
    };
  };
} ControlFrame;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct long_frame {
  FrameHeader    header;
  unsigned short length;
  union {
    unsigned char spayload[FRAME_MAX_SIZE + WS_MASK_SIZE];
    struct {
      unsigned char mask[WS_MASK_SIZE];
      unsigned char payload[FRAME_MAX_SIZE];
    };
  };
} LongFrame;
#pragma pack(pop)

/*
NOTE:
No need for Long Long Frame implementation (with length longer than 65536) : because that's insane!
If very huge payloads need to be transmitted, use the fractionning functionnality. But anyways very
big payloads are not the focus of this implementation.
*/

/*
NOTE:
This implementation is not optimized for multicast at all. For now, simply send the message to each
client separetly.
*/
void wsmulticast(WebSocketServer *server, const void *buffer, const size_t size, const int type);
void wsping(WebSocketServer *server, int client);

void wswrite(WebSocketServer *server, const int client, const unsigned char *buffer, const size_t size, const int type);
int  wsread(WebSocketServer *server, const int client, unsigned char *buffer, const size_t maxbytes, size_t *readbytes);

int  wsaccept(WebSocketServer *server);
void wsclose(WebSocketServer *server, int client);

WebSocketServer *wsstart(const short port, FILE *messages, FILE *errors);
void             wsstop(WebSocketServer *server);

#endif