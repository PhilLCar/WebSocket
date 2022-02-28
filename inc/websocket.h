/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 28 Feb 2022
 * Description: WebSocket server implementation for C.
 */

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <pthread.h>
#include <time.h>

/*
NOTE: 
This is designed for a Little Endian system (Intel), it will require some redesign if used on a Big Endian. 
*/

// The maximum number of client that can connect to the WebSocket server
#define WS_MAX_CONN            32
#define WS_CHECK_PERIOD_US  15000

#define COM_BUFFERS_SIZE 65536

#define FRAME_MAX_SIZE   65536
#define FRAME_CONTINUE     0x0
#define FRAME_TEXT         0x1
#define FRAME_BINARY       0x2
#define FRAME_CLOSE        0x8
#define FRAME_PING         0x9
#define FRAME_PONG         0xA

typedef struct interface {
  int out_ptr;
  int in_ptr;
  unsigned char out[COM_BUFFERS_SIZE];
  unsigned char in[COM_BUFFERS_SIZE];
  pthread_mutex_t in_lock;
  pthread_mutex_t out_lock;
  // "private" part
  short           _port = 0;
  long            _arg[WS_MAX_CONN];
  int             _clientstop[WS_MAX_CONN];
  pthread_t       _clientthreadI[WS_MAX_CONN];
  pthread_t       _clientthreadO[WS_MAX_CONN];
  pthread_mutex_t _clientmutex[WS_MAX_CONN];
  struct timeval  _timeout;
  pthread_t       _serverthread = 0;
} Interface;

// This struct is a little bit weird
// It doesn't fit the doc for hton reasons
#pragma pack(push, 1)
typedef struct frame_header {
  unsigned int  opcode : 4;
  unsigned int  rsv3   : 1;
  unsigned int  rsv2   : 1;
  unsigned int  rsv1   : 1;
  unsigned int  end    : 1;
  unsigned int  length : 7;
  unsigned int  mask   : 1;
} FrameHeader;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct control_frame {
  FrameHeader header;
  union {
    unsigned char spayload[129];
    struct {
      unsigned char mask[4];
      unsigned char payload[125];
    };
  };
} ControlFrame;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct long_frame {
  FrameHeader    header;
  unsigned short length;
  union {
    unsigned char spayload[FRAME_MAX_SIZE - 4];
    struct {
      unsigned char mask[4];
      unsigned char payload[FRAME_MAX_SIZE - 8];
    };
  };
} LongFrame;
#pragma pack(pop)

// No very big frames : because that's insane!

typedef struct frame {
  FrameHeader   header;
  unsigned char mask[4];
  unsigned int  length;
  unsigned char payload[FRAME_MAX_SIZE];
} Frame;

void mempush(Interface *dest, const void *src, const size_t size);
void mempull(void *dest, const Interface *src, int *from, const size_t size);

Interface *startservice(const short port);
void       stopservice(Interface *interface);

#endif