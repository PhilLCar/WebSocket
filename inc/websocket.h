/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 28 Feb 2022
 * Description: WebSocket server implementation for C.
 * Standard: https://datatracker.ietf.org/doc/html/rfc6455
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <pthread.h>
#include <time.h>

/*
NOTE: 
This is designed for a Little Endian system (Intel), it hasn't been tested on a Big Endian system.
*/

// The maximum number of client that can connect to the WebSocket server
#define WS_MAX_CONN            32
#define WS_CHECK_PERIOD_US  15000

#define COM_BUFFERS_SIZE (sizeof(LongFrame) << 2)

/*
NOTE:
Maybe for safety the frame size should be a smaller fraction of the com buffer, but let's assume
we never receive more than 4 Long Frames within the same cycle.
*/
#define FRAME_MAX_SIZE     65536
#define FRAME_CONTROL_SIZE   125
#define FRAME_MASK_SIZE        4
#define FRAME_CONTINUE       0x0
#define FRAME_TEXT           0x1
#define FRAME_BINARY         0x2
#define FRAME_CLOSE          0x8
#define FRAME_PING           0x9
#define FRAME_PONG           0xA

typedef struct interface {
  short           port;
  int             timeout_ms;
  int             mask;
  Connection     *connections[WS_MAX_CONN];
  pthread_t       serverthread;
} Interface;

/*
NOTE:
The idea behind the rolling buffers is to minimize the malloc/free calls.
A simpler implementation would have a simple stack of memory pointers, but that could
lead to a very frequent malloc/free use. This was developped with the use-case of
sending small frames at a very high frequency in mind, and in this case the rolling
buffers should provide a marginal advantage.
*/
typedef struct connection {
  struct rolling_buffer {
    int              index;
    unsigned char    buffer[COM_BUFFERS_SIZE];
    pthread_mutex_t  lock;
    pthread_t        thread;
  };
  struct rolling_buffer input;
  struct rolling_buffer output;
  int                   update;
  int                   fd;
  struct timeval        timeout;
  pthread_mutex_t       lock;
  int                   stop;
  long                  ping;
  int                   mask;
} Connection;

// TODO: untested, byte order might be wrong
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
    unsigned char spayload[FRAME_CONTROL_SIZE + FRAME_MASK_SIZE];
    struct {
      unsigned char mask[FRAME_MASK_SIZE];
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
    unsigned char spayload[FRAME_MAX_SIZE + FRAME_MASK_SIZE];
    struct {
      unsigned char mask[FRAME_MASK_SIZE];
      unsigned char payload[FRAME_MAX_SIZE];
    };
  };
} LongFrame;
#pragma pack(pop)

/*
NOTE:
No need for long long frames implementation (with length longer than 65536) : because that's insane!
If very huge payloads need to be transmitted, use the fractionning functionnality.
*/

typedef struct frame {
  FrameHeader   header;
  unsigned char mask[FRAME_MASK_SIZE];
  unsigned int  length;
  unsigned char payload[FRAME_MAX_SIZE];
} Frame;

/*
NOTE:
This implementation is not optimized for multicast at all. For now, simply send the message to each
client separetly.
*/
int  multicast(Interface *dest, const void *src, const size_t size);

int  webping(Connection *dest);

/*
NOTE:
Because of the threading, it's necessary to copy the data so that the calling thread can release it
and the server thread can use it when it pleases.
*/
void webpush(Connection *dest, const void *src, const size_t size, int binary);
int  webpull(void *dest, Connection *src, int *from);

Interface *startservice(const short port, int timeout, int mask);
void       stopservice(Interface *interface);

#endif