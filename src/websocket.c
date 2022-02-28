/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 28 Feb 2022
 * Description: WebSocket server implementation for C.
 */

#include <websocket.h>
#include <http.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SLEEP_PERIOD_S       1

const char *SOCKET_MAGIC_STR = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
unsigned char *encode64(const unsigned char *input, int length) {
  const int pl = 4 * ((length + 2) / 3);
  unsigned char *output = malloc((pl + 1) * sizeof(char));
  EVP_EncodeBlock(output, input, length);
  return output;
}
// https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
unsigned char *decode64(const unsigned char *input, int length) {
  const int pl = 3 * length / 4;
  unsigned char *output = malloc((pl + 1) * sizeof(char));
  EVP_DecodeBlock(output, input, length);
  return output;
}

int handshake(Interface *interface, int fd, int me) {
  const int bufsize = 4096;
  char buffer[bufsize];
  char key[64];
  int  version;
  HttpRequest  request;
  HttpResponse response;
  memset(buffer, 0, bufsize * sizeof(char));
  pthread_mutex_lock(&interface->in_lock);
  read(fd, buffer, bufsize);
  pthread_mutex_unlock(&interface->in_lock);
  printf("%s\n", buffer);
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
  key[0] = 0;
  getfield(request.header, "Sec-WebSocket-Key", key);
  getfield(request.header, "Sec-WebSocket-Version", buffer);
  if (!key[0]) {
    getfield(request.header, "Sec-Websocket-Key", key);
    getfield(request.header, "Sec-Websocket-Version", buffer);
  }
  version = atoi(buffer);
  printf("Connecting to socket with key: %s\n", key);
  printf("WebSocket version:             %d\n", version);

  // Response
  unsigned char *resp = malloc((strlen(key) + strlen(SOCKET_MAGIC_STR) + 1) * sizeof(char));
  unsigned char  digest[SHA_DIGEST_LENGTH];
  unsigned char *ans;
  int   i;

  for (i = 0; key[i]; i++) resp[i] = key[i];
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
  pthread_mutex_lock(&interface->_clientmutex[me]);
  write(fd, buffer, strlen(buffer));
  pthread_mutex_unlock(&interface->_clientmutex[me]);

  free(resp);
  free(ans);
  free(request.body);
  return 0;
}

void *_cliento(void *vargp) {
  Interface *interface = (Interface*)vargp;
  int me = (long)vargp >> 32;
  int fd = (long)vargp & 0xFFFFFFFF;
  int lastpos;
  int opcode = 1;

  // init the last position pointer
  pthread_mutex_lock(&interface->out_lock);
  lastpos = interface->out_ptr;
  pthread_mutex_unlock(&interface->out_lock);

  while (!interface->_clientstop[me]) {
    if (lastpos != interface->out_ptr) {
      pthread_mutex_lock(&interface->out_lock);
      int size = interface->out_ptr - lastpos;
      int last = 1;
      if (size < 0) size += COM_BUFFERS_SIZE;
      do {
        short length;
        int   uid;
        for (int i = 0; i < sizeof(short); i++) {
          ((char*)&length)[i] = interface->out[lastpos];
          lastpos = (lastpos + 1) % COM_BUFFERS_SIZE;
        }
        for (int i = 0; i < sizeof(int); i++) {
          ((char*)&uid)[i] = interface->out[lastpos];
          lastpos = (lastpos + 1) % COM_BUFFERS_SIZE;
        }
        if (length < 0) {
          length = -length;
          last   = 0;
        }
        length -= sizeof(short) + sizeof(int);
        if (fd == uid || uid == -1) {
          if (length < 126) { // use a cframe
            ControlFrame cframe;
            memset(&cframe.header, 0, sizeof(FrameHeader));
            cframe.header.end  = last;
            cframe.header.mask = 0;
            cframe.header.length = length;
            cframe.header.opcode = opcode ? FRAME_BINARY : 0;
            for (int i = 0; i < length; i++) {
              cframe.spayload[i] = interface->out[lastpos];
              lastpos = (lastpos + 1) % COM_BUFFERS_SIZE;
            }
            pthread_mutex_lock(&interface->_clientmutex[me]);
            write(fd, &cframe, length + 2);
            pthread_mutex_unlock(&interface->_clientmutex[me]);
          } else {
            LongFrame lframe;
            memset(&lframe.header, 0, sizeof(FrameHeader));
            lframe.header.end  = last;
            lframe.header.mask = 0;
            lframe.header.length = 126;
            lframe.header.opcode = opcode ? FRAME_BINARY : 0;
            lframe.length = htons(length);
            for (int i = 0; i < length; i++) {
              lframe.spayload[i] = interface->out[lastpos];
              lastpos = (lastpos + 1) % COM_BUFFERS_SIZE;
            }
            pthread_mutex_lock(&interface->_clientmutex[me]);
            write(fd, &lframe, length + 4);
            pthread_mutex_unlock(&interface->_clientmutex[me]);
          }
          opcode = last;
        } else lastpos = (lastpos + length) % COM_BUFFERS_SIZE;
        size -= length + sizeof(short) + 2 * sizeof(int);
      } while (size > 0);
      if (lastpos != interface->out_ptr) {
        fprintf(stderr, "Fatal synchronization error! (output buffer)\n");
        exit(EXIT_FAILURE);
      }
      pthread_mutex_unlock(&interface->out_lock);
    }
    usleep(WS_CHECK_PERIOD_US);
  }
  return NULL;
}

void *_clienti(void *vargp) {
  Interface *
  int          me  = (long)vargp >> 32;
  int          fd  = (long)vargp & 0xFFFFFFFF;
  int          cop = 0;
  Frame        frame;
  ControlFrame cframe;

  _clientstop[me] = handshake(fd, me);

  if (!_clientstop[me]) {
    pthread_create(&_clientthreadO[me], NULL, _cliento, vargp);
  }

  while (!_clientstop[me]) {
    fd_set input;
    FD_ZERO(&input);
    FD_SET(fd, &input);
    int n = select(fd + 1, &input, NULL, NULL, &_timeout);
    if (n < 0) {
      _clientstop[me] = 1; break;
    } else if (n == 0) continue;
    pthread_mutex_lock(&_clientmutex[me]);
    read(fd, &frame.header, sizeof(FrameHeader));
    if (frame.header.length < 126) {
      frame.length = frame.header.length;
    } else if (frame.header.length == 126) {
      short l;
      read(fd, &l, 2);
      frame.length = ntohs(l);
    } else if (frame.header.length == 127) {
      int l;
      read(fd, &l, 4);
      frame.length = ntohl(l);
    }
    if (frame.header.mask) {
      read(fd, frame.mask, 4);
    } else {
      memset(frame.mask, 0, 4 * sizeof(char));
    }
    read(fd, frame.payload, frame.length);
    pthread_mutex_unlock(&_clientmutex[me]);
    switch (frame.header.opcode) {
      case FRAME_CONTINUE:
        switch(cop) {
          case FRAME_TEXT:
            goto CASE_FRAME_TEXT;
            break;
          case FRAME_BINARY:
            goto CASE_FRAME_BINARY;
            break;
          default:
            fprintf(stderr, "Received a continued frame without previous opcode!\n");
            break;
        }
        break;
      case FRAME_TEXT:
      CASE_FRAME_TEXT:
        if (!frame.header.end) cop = FRAME_TEXT;
        else                   cop = 0;
        printf("Message received: %s\n", frame.payload);
        break;
      case FRAME_BINARY:
      CASE_FRAME_BINARY:
        if (!frame.header.end) cop = FRAME_TEXT;
        else                   cop = 0;
        // Update the inputs
        pthread_mutex_lock(&_interface.in_lock);
        int   ptr        = _interface.in_ptr;
        short chunk_size = frame.length + sizeof(short) + sizeof(int);
        if (cop) chunk_size = -chunk_size;
        for (int i = 0; i < sizeof(short); i++) {
          _interface.in[ptr] = ((char*)&chunk_size)[i];
          ptr = (ptr + 1) % COM_BUFFERS_SIZE;
        }
        for (int i = 0; i < sizeof(int); i++) {
          _interface.in[ptr] = ((char*)&fd)[i];
          ptr = (ptr + 1) % COM_BUFFERS_SIZE;
        }
        for (int i = 0; i < frame.length; i++) {
          _interface.in[ptr] = frame.payload[i] ^ frame.mask[i % 4];
          ptr = (ptr + 1) % COM_BUFFERS_SIZE;
        }
        _interface.in_ptr = ptr;
        pthread_mutex_unlock(&_interface.in_lock);
        break;
      case FRAME_CLOSE:
        _clientstop[me] = 1;
        memset(&cframe, 0, sizeof(ControlFrame));
        cframe.header.opcode = FRAME_CLOSE;
        cframe.header.end    = 1;
        cframe.header.mask   = 0;
        cframe.header.length = frame.header.length;
        for (int i = 0; i < frame.header.length; i++) {
          cframe.spayload[i] = frame.payload[i] ^ frame.mask[i % 4];
        }
        pthread_mutex_lock(&_clientmutex[me]);
        write(fd, &cframe, sizeof(FrameHeader) + 2);
        pthread_mutex_unlock(&_clientmutex[me]);
        break;
      case FRAME_PING:
        memset(&cframe, 0, sizeof(ControlFrame));
        cframe.header.opcode = FRAME_PONG;
        cframe.header.end    = 1;
        cframe.header.mask   = 0;
        cframe.header.length = frame.header.length;
        for (int i = 0; i < frame.header.length; i++) {
          cframe.spayload[i] = frame.payload[i] ^ frame.mask[i % 4];
        }
        pthread_mutex_lock(&_clientmutex[me]);
        write(fd, &cframe, sizeof(FrameHeader) + 2);
        pthread_mutex_unlock(&_clientmutex[me]);
        break;
      case FRAME_PONG:
        // we don't really care...
        break;
      default:
        fprintf(stderr, "Unimplemented!\n");
        exit(EXIT_FAILURE);
        break;
    }
  }
  if (_clientthreadO[me]) {
    pthread_join(_clientthreadO[me], NULL);
    _clientthreadO[me] = 0;
  }
  close(fd);
  return NULL;
}

void *_server(void *vargp) {
  Interface *interface = (Interface*)vargp;
  int  server_fd;
  int  client_fd;
  struct sockaddr_in address;
  socklen_t          addrlen = sizeof(struct sockaddr_in);

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    fprintf(stderr, "Cannot create socket\n");
    return NULL;
  }
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    fprintf(stderr, "Cannot reuse socket");
    return NULL;
  }

  memset(&address, 0, sizeof(struct sockaddr_in));
  memset(address.sin_zero, 0, sizeof(unsigned char));
  address.sin_family      = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port        = htons(interface->_port);

  if (bind(server_fd, (struct sockaddr *restrict)&address, sizeof(struct sockaddr_in)) < 0) {
    fprintf(stderr, "Bind failed\n");
    return NULL;
  }

  if (listen(server_fd, WS_MAX_CONN) < 0) {
    fprintf(stderr, "Cannot listen\n");
    return NULL;
  }

  while(1) {
    printf("Waiting for new connection\n");
    if ((client_fd = accept(server_fd, (struct sockaddr *restrict)&address, &addrlen)) < 0) {
      fprintf(stderr, "Cannot accept\n");
      return NULL;
    }

    int success = 0;
    for (int i = 0; i < WS_MAX_CONN; i++) {
      if (interface->_clientthreadI[i] && interface->_clientstop[i]) { // reclaim a dead thread
        pthread_join(interface->_clientthreadI[i], NULL);
        interface->_clientthreadI[i] = 0;
        interface->_clientstop[i]   = 0;

      }
      if (!interface->_clientthreadI[i]) {
        long arg = ((long)i << 32) | client_fd;
        printf("Connecting client #%d...\n\n", i);
        pthread_create(&interface->_clientthreadI[i], NULL, _clienti, (void*)arg);
        success = 1;
        break;
      }
    }
    if (success) printf("Connection success\n");
    else         fprintf(stderr, "Max connections reached\n");
  }
  return NULL;
}

void mempush(Interface *dest, const void *src, const size_t size) {
  size_t stop1 = dest->out_ptr + size;
  size_t stop2 = 0;
  size_t size1 = size;
  if (stop1 >= COM_BUFFERS_SIZE) {
    stop2 = stop1 - COM_BUFFERS_SIZE;
    stop1 = 0;
    size1 = COM_BUFFERS_SIZE - dest->out_ptr;
  }
  memcpy(&dest->out[dest->out_ptr], src, size1);
  dest->out_ptr = stop1;
  if (stop2) {
    memcpy(&dest->out[0], src + size1, stop2);
    dest->out_ptr = stop2;
  }
}

void mempull(void *dest, const Interface *src, int *from, const size_t size) {
  size_t stop1 = *from + size;
  size_t stop2 = 0;
  size_t size1 = size;
  if (stop1 >= COM_BUFFERS_SIZE) {
    stop2 = stop1 - COM_BUFFERS_SIZE;
    stop1 = 0;
    size1 = COM_BUFFERS_SIZE - *from;
  }
  memcpy(dest, &src->in[*from], size1);
  *from = stop1;
  if (stop2) {
    memcpy(dest, &src->in[0], stop2);
    *from = stop2;
  }
}

Interface *startservice(const short port) {
  Interface *interface = malloc(sizeof(Interface));
  if (interface) {
    interface->out_ptr = 0;
    interface->in_ptr  = 0;
    memset(interface->out, 0, COM_BUFFERS_SIZE * sizeof(char));
    memset(interface->in,  0, COM_BUFFERS_SIZE * sizeof(char));
    pthread_mutex_init(&interface->in_lock,  NULL);
    pthread_mutex_init(&interface->out_lock, NULL);

    interface->_port = port;
    for (int i = 0; i < WS_MAX_CONN; i++) {
      interface->_clientstop[i]    = 0;
      interface->_clientthreadI[i] = 0;
      interface->_clientthreadO[i] = 0;
      pthread_mutex_init(&interface->_clientmutex[i], NULL);
    }
    interface->_timeout.tv_sec  = SLEEP_PERIOD_S;
    interface->_timeout.tv_usec = 0;
    pthread_create(&interface->_serverthread, NULL, _server, (void*)interface);
  }
  return interface;
}

void stopservice(Interface *interface) {
  if (interface) {
    pthread_cancel(interface->_serverthread);
    pthread_join(interface->_serverthread, NULL);
    for (int i = 0; i < WS_MAX_CONN; i++) {
      if (interface->_clientthreadI[i]) {
        interface->_clientstop[i] = 1;
        pthread_join(interface->_clientthreadI[i], NULL);
        interface->_clientthreadI[i] = 0;
      }
    }
    free(interface);
  }
}