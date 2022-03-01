/* Author: Philippe Caron (philippe-caron@hotmail.com)
 * Date: 28 Feb 2022
 * Description: WebSocket server implementation for C.
 * Standard: https://datatracker.ietf.org/doc/html/rfc6455
 */

#include <websocket.h>
#include <http.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

const char *SOCKET_MAGIC_STR = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

void writel(unsigned char *buffer, int buffer_length, int *position, long value) {
  for (int i = 0; i < sizeof(long); i++) {
    buffer[*position] = (value >> (i << 3)) & 0xFF;
    *position = (*position + 1) % buffer_length;
  }
}

long readl(unsigned char *buffer, int buffer_length, int *position) {
  long value;
  for (int i = 0; i < sizeof(long); i++) {
    value += (buffer[*position] << (i << 3));
    *position = (*position + 1) % buffer_length;
  }
  return value;
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

int handshake(Connection *connection) {
  const int bufsize = 4096;
  char buffer[bufsize];
  char key[64];
  int  version;
  HttpRequest  request;
  HttpResponse response;
  memset(buffer, 0, bufsize * sizeof(char));
  pthread_mutex_lock(&connection->lock);
  read(connection->fd, buffer, bufsize);
  pthread_mutex_unlock(&connection->lock);
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
  pthread_mutex_lock(&connection->lock);
  write(connection->fd, buffer, strlen(buffer));
  pthread_mutex_unlock(&connection->lock);

  free(resp);
  free(ans);
  free(request.body);
  return 0;
}

void *clientoutput(void *vargp) {
  Connection *connection = (Connection*)vargp;
  int         lastpos;
  int         total;

  int              fd       = connection->fd;
  pthread_mutex_t *lock     = &connection->lock;
  pthread_mutex_t *lock_out = &connection->output.lock;
  unsigned char   *buffer   = connection->output.buffer;

  // Initiate the last position pointer
  pthread_mutex_lock(lock_out);
  lastpos = connection->output.index;
  pthread_mutex_unlock(lock_out);

  while (!connection->stop) {
    while (lastpos != connection->output.index) {
      pthread_mutex_lock(lock_out);
      {
        unsigned char mask[FRAME_MASK_SIZE];
        long size   = readl(buffer, COM_BUFFERS_SIZE, lastpos);
        long opcode = readl(buffer, COM_BUFFERS_SIZE, lastpos);
        int  masked = connection->mask != 0;
        int  end    = 1;

        memset(&mask, 0, FRAME_MASK_SIZE * sizeof(unsigned char));
        if (masked) {
          for (int i = 0; i < FRAME_MASK_SIZE; i++) {
            mask[i] = 0xFF & (connection->mask >> (i << 3));
          }
        }

        if (size < 0) {
          end  = 0;
          size = -size;
        }
        total += size;
        if (total > COM_BUFFERS_SIZE) {
          // This is an infinite loop now
          fprintf(stderr, "Fatal synchronization error (output buffer)");
          exit(EXIT_FAILURE);
        }
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
            for (i = 0; i < FRAME_MASK_SIZE; i++) {
              cframe.spayload[i] = mask[i];
            }
          }
          pthread_mutex_lock(lock);
          write(fd, &cframe, size + sizeof(FrameHeader) + (masked ? FRAME_MASK_SIZE : 0));
          pthread_mutex_unlock(lock);
          connection->ping = clock();
        } 
        // If it fits in a Control Frame
        else if (size <= FRAME_CONTROL_SIZE) {
          ControlFrame  cframe;
          int  i = 0;

          memset(&cframe.header, 0, sizeof(FrameHeader));
          cframe.header.end    = 1;
          cframe.header.mask   = masked;
          cframe.header.length = size;
          cframe.header.opcode = opcode;
          cframe.header.bytes  = htons(cframe.header.bytes);
          if (masked) {
            for (i = 0; i < FRAME_MASK_SIZE; i++) {
              cframe.spayload[i] = mask[i];
            }
          }
          for (; i < size; i++) {
            cframe.spayload[i] = buffer[lastpos] ^ mask[i % FRAME_MASK_SIZE];
            lastpos = (lastpos + 1) % COM_BUFFERS_SIZE;
          }
          pthread_mutex_lock(lock);
          write(fd, &cframe, size + sizeof(FrameHeader) + (masked ? FRAME_MASK_SIZE : 0));
          pthread_mutex_unlock(lock);
        }
        // If it fits in a Long Frame
        else if (size <= FRAME_MAX_SIZE) {
          LongFrame lframe;
          int i = 0;

          memset(&lframe.header, 0, sizeof(FrameHeader));
          lframe.header.end    = 1;
          lframe.header.mask   = masked;
          lframe.header.length = 126;
          lframe.header.opcode = opcode;
          lframe.header.bytes  = htons(lframe.header.bytes);
          lframe.length        = htons((unsigned short)size);
          if (masked) {
            for (i = 0; i < FRAME_MASK_SIZE; i++) {
              lframe.spayload[i] = mask[i];
            }
          }
          for (; i < size; i++) {
            lframe.spayload[i] = buffer[lastpos] ^ mask[i % FRAME_MASK_SIZE];
            lastpos = (lastpos + 1) % COM_BUFFERS_SIZE;
          }
          pthread_mutex_lock(lock);
          write(fd, &lframe, size + sizeof(FrameHeader) + sizeof(unsigned short) + (masked ? FRAME_MASK_SIZE : 0));
          pthread_mutex_unlock(lock);
        } else {
          unsigned char *content = (void*)readl(buffer, COM_BUFFERS_SIZE, lastpos);
          int            index   = 0;
          int            frames  = size / FRAME_MAX_SIZE;
          for (int j = 0; j < frames; j++) {
            LongFrame lframe;
            int last  = j == (frames - 1);
            int i     = 0;
            int fsize = last ? size % FRAME_MAX_SIZE : FRAME_MAX_SIZE;

            memset(&lframe.header, 0, sizeof(FrameHeader));
            lframe.header.end    = last;
            lframe.header.mask   = masked;
            lframe.header.length = 126;
            lframe.header.opcode = opcode;
            lframe.header.bytes  = htons(lframe.header.bytes);
            lframe.length        = htons((unsigned short)size);
            if (masked && !j) {
              for (i = 0; i < FRAME_MASK_SIZE; i++) {
                lframe.spayload[i] = mask[i];
              }
            }
            for (; i < size; i++) {
              lframe.spayload[i] = content[FRAME_MAX_SIZE * j + i] ^ mask[i % FRAME_MASK_SIZE];
              lastpos = (lastpos + 1) % COM_BUFFERS_SIZE;
            }
            pthread_mutex_lock(lock);
            write(fd, &lframe, size + sizeof(FrameHeader) + sizeof(unsigned short) + (masked && !j ? FRAME_MASK_SIZE : 0));
            pthread_mutex_unlock(lock);
          }
          free(content);
        }
      }
      pthread_mutex_unlock(lock_out);
    }
    sleep(WS_CHECK_PERIOD_US);
  }
  return NULL;
}

void *clientinput(void *vargp) {
  Connection     *connection = (Connection*)vargp;
  int             expect     = 0;
  Frame           frame;
  ControlFrame    cframe;
  int             opcode;

  struct timeval  *timeout    = &connection->timeout;
  int              fd         = connection->fd;
  pthread_mutex_t *lock       = &connection->lock;
  pthread_mutex_t *lock_input = &connection->input.lock;

  unsigned char *bigbuffer = NULL;
  int            bigcap    = FRAME_MAX_SIZE;
  int            bigsize   = 0;

  connection->stop = handshake(connection);

  if (!connection->stop) {
    pthread_create(&connection->output.thread, NULL, clientoutput, vargp);
  }

  while (!connection->stop) {
    fd_set input;
    FD_ZERO(&input);
    FD_SET(fd, &input);
    int n = select(fd + 1, &input, NULL, NULL, timeout);
    if (n < 0) {
      connection->stop = 1; break;
    } else if (n == 0) continue;
    pthread_mutex_lock(lock);
    {
      short header;
      read(fd, &header, sizeof(short));
      frame.header.bytes = ntohs(header);
    }
    if (frame.header.length < 126) {
      frame.length = frame.header.length;
    } else if (frame.header.length == 126) {
      short l;
      read(fd, &l, sizeof(short));
      frame.length = ntohs(l);
    } else if (frame.header.length == 127) {
      // Unsupported: Long Long Frame
      int l;
      read(fd, &l, sizeof(int));
      frame.length = ntohl(l);
    }
    if (frame.header.mask) {
      read(fd, frame.mask, FRAME_MASK_SIZE);
    } else {
      memset(frame.mask, 0, FRAME_MASK_SIZE * sizeof(unsigned char));
    }
    read(fd, frame.payload, frame.length);
    pthread_mutex_unlock(lock);
    switch (frame.header.opcode) {
      case FRAME_CONTINUE:
      case FRAME_TEXT:
      case FRAME_BINARY:
        if (frame.header.opcode == FRAME_CONTINUE && !expect) {
          fprintf(stderr, "Received a continued frame without previous opcode!\n");
          break;
        } else if (frame.header.opcode != FRAME_CONTINUE) {
          opcode = frame.header.opcode;
        }
        expect = !frame.header.end;
        // First partial frame
        if (expect || bigbuffer) {
          if (!bigbuffer) bigbuffer = malloc(bigcap * sizeof(unsigned char));
          if (frame.length + bigsize > bigcap) {
            unsigned char *tmp = realloc(bigbuffer, bigcap <<= 1);
            if (!tmp) {
              free(bigbuffer);
              bigcap    = FRAME_MAX_SIZE;
              bigsize   = 0;
              fprintf(stderr, "Insufficient memory to receive large payload (fragmented frames)\n");
            } else bigbuffer = tmp;
          }
          if (!bigbuffer) {
            fprintf(stderr, "Memory allocation error while trying to receive large payload (fragmented frames)\n");
          }
          for (int i = 0; i < frame.length; i++) bigbuffer[bigsize++] = frame.payload[i] ^ frame.mask[i % 4];
          // The end of the fragmented message
          if (!expect) {
            pthread_mutex_lock(lock_input);
            writel(connection->input.buffer, COM_BUFFERS_SIZE, &connection->input.index, -(long)bigsize);
            writel(connection->input.buffer, COM_BUFFERS_SIZE, &connection->input.index, (long)opcode);
            writel(connection->input.buffer, COM_BUFFERS_SIZE, &connection->input.index, (long)(void*)bigbuffer);
            connection->update += bigsize + 2 * sizeof(long);
            pthread_mutex_unlock(lock_input);
            bigbuffer = NULL;
            bigcap    = FRAME_MAX_SIZE;
            bigsize   = 0;
          }
        }
        // Not a partial frame
        else {
          // Update the inputs
          pthread_mutex_lock(lock_input);
          {
            writel(connection->input.buffer, COM_BUFFERS_SIZE, &connection->input.index, (long)frame.length);
            writel(connection->input.buffer, COM_BUFFERS_SIZE, &connection->input.index, (long)opcode);
            {
              unsigned char *buffer = connection->input.buffer;
              int index             = connection->input.index;
              if (frame.header.mask) {
                for (int i = 0; i < frame.length; i++) {
                  buffer[index] = frame.payload[i] ^ frame.mask[i % FRAME_MASK_SIZE];
                  index = (index + 1) % COM_BUFFERS_SIZE;
                }
              } else {
                for (int i = 0; i < frame.length; i++) {
                  buffer[index] = frame.payload[i];
                  index = (index + 1) % COM_BUFFERS_SIZE;
                }
              }
              connection->input.index  = index;
              connection->update      += frame.length + 2 * sizeof(long);
            }
          }
          pthread_mutex_unlock(lock_input);
        }
        break;
      case FRAME_CLOSE:
        connection->stop = 1;
        memset(&cframe, 0, sizeof(ControlFrame));
        cframe.header.opcode = FRAME_CLOSE;
        cframe.header.end    = 1;
        cframe.header.mask   = frame.header.mask;
        cframe.header.length = 0;
        pthread_mutex_lock(lock);
        write(fd, &cframe, sizeof(FrameHeader));
        if (frame.header.mask) {
          write(fd, &frame.mask, FRAME_MASK_SIZE);
        }
        pthread_mutex_unlock(lock);
        break;
      case FRAME_PING:
        memset(&cframe, 0, sizeof(ControlFrame));
        cframe.header.opcode = FRAME_PONG;
        cframe.header.end    = 1;
        cframe.header.mask   = frame.header.mask;
        cframe.header.length = 0;
        pthread_mutex_lock(lock);
        write(fd, &cframe, sizeof(FrameHeader));
        if (frame.header.mask) {
          write(fd, &frame.mask, FRAME_MASK_SIZE);
        }
        pthread_mutex_unlock(lock);
        break;
      case FRAME_PONG:
        connection->ping = (clock() - connection->init) / (CLOCKS_PER_SEC / 1000);
        break;
      default:
        fprintf(stderr, "Unimplemented!\n");
        exit(EXIT_FAILURE);
        break;
    }
  }
  if (connection->output.thread) {
    pthread_join(connection->output.thread, NULL);
    connection->output.thread = 0;
  }
  close(fd);
  return NULL;
}

void *serve(void *vargp) {
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
  address.sin_port        = htons(interface->port);

  if (bind(server_fd, (struct sockaddr *restrict)&address, sizeof(struct sockaddr_in)) < 0) {
    fprintf(stderr, "Bind failed\n");
    return NULL;
  }

  if (listen(server_fd, WS_MAX_CONN) < 0) {
    fprintf(stderr, "Cannot listen\n");
    return NULL;
  }

  while(1) {
    printf("%s\n", "Waiting for new connection");
    if ((client_fd = accept(server_fd, (struct sockaddr *restrict)&address, &addrlen)) < 0) {
      fprintf(stderr, "Cannot accept\n");
      return NULL;
    }

    int success = 0;
    for (int i = 0; i < WS_MAX_CONN; i++) {
      // reclaim a dead thread
      if (interface->connections[i] && interface->connections[i]->stop) {
        Connection *connection = interface->connections[i];
        pthread_join(connection->input.thread, NULL);
        pthread_mutex_destroy(&connection->input.lock);
        pthread_mutex_destroy(&connection->output.lock);
        pthread_mutex_destroy(&connection->lock);
        free(connection);
        interface->connections[i] = NULL;
      }
      if (!interface->connections[i]) {
        Connection *connection = malloc(sizeof(Connection));
        memset(connection, 0, sizeof(Connection));
        connection->fd              = client_fd;
        connection->timeout.tv_sec  =  interface->timeout_ms / 1000;
        connection->timeout.tv_usec = (interface->timeout_ms % 1000) * 1000;
        connection->mask            = interface->mask;
        connection->input.fd        = open(COM_TMP_PATH, __O_TMPFILE | O_RDWR | O_EXCL);
        pthread_mutex_init(&connection->input.lock, NULL);
        pthread_mutex_init(&connection->input.lock, NULL);
        pthread_mutex_init(&connection->lock, NULL);
        printf("Connecting client #%d...\n\n", i);
        pthread_create(&connection->input.thread, NULL, clientinput, (void*)connection);
        interface->connections[i] = connection;
        success = 1;
        break;
      }
    }
    if (success) printf("%s\n", "Connection success");
    else         fprintf(stderr, "Max connections reached\n");
  }
  return NULL;
}

void multicast(Interface *dest, const void *src, const size_t size, int type) {
  for (int i = 0; i < WS_MAX_CONN; i++) {
    if (dest->connections[i]) webpush(dest->connections[i], src, size, type);
  }
}

int webping(Connection *dest, const int timeout) {
  int ping = -1;
  dest->ping = 0;
  webpush(dest, NULL, 0, 0);
  for (int i = 0; !timeout || i < timeout / (WS_CHECK_PERIOD_US / 1000); i++) {
    usleep(WS_CHECK_PERIOD_US);
    if (dest->ping) {
      ping = dest->ping;
      dest->ping = 0;
      break;
    }
  }
  return ping;
}

void webpush(Connection *dest, const void *src, const size_t size, int type) {
  pthread_mutex_lock(&dest->output.lock);
  {
    if (size > FRAME_MAX_SIZE) {
      void *content = malloc(size * sizeof(unsigned char));
      if (content) {
        writel(dest->output.buffer, COM_BUFFERS_SIZE, &dest->output.index, -(long)size);
        writel(dest->output.buffer, COM_BUFFERS_SIZE, &dest->output.index, (long)type);
        writel(dest->output.buffer, COM_BUFFERS_SIZE, &dest->output.index, (long)content);
      }
    } else {
      writel(dest->output.buffer, COM_BUFFERS_SIZE, &dest->output.index, (long)size);
      writel(dest->output.buffer, COM_BUFFERS_SIZE, &dest->output.index, (long)type);
      {
        unsigned char *buffer = dest->output.buffer;
        int            index  = dest->output.index;
        size_t         stop1  = index + size;
        size_t         stop2  = 0;
        size_t         size1  = size;
        if (stop1 >= COM_BUFFERS_SIZE) {
          stop2 = stop1 - COM_BUFFERS_SIZE;
          stop1 = 0;
          size1 = COM_BUFFERS_SIZE - dest->output.index;
        }
        memcpy(&buffer[index], src, size1 * sizeof(unsigned char));
        dest->output.index = stop1;
        if (stop2) {
          memcpy(&buffer[0], src + size1, stop2 * sizeof(unsigned char));
          dest->output.index = stop2;
        }
      }
    }
  }
  pthread_mutex_unlock(&dest->output.lock);
}

int webpull(void *dest, Connection *src, int *from) {
  long type = 0;
  if (src->update) {
    pthread_mutex_lock(&src->input.lock);
    if (src->update > COM_BUFFERS_SIZE) {
      // This will create a memory leak if the COM buffer overruns while a pointer is on it
      // It's the responsability of the program to diligently call webpull()
      src->update = 0;
      *from = src->input.index;
      printf("%s\n", "Warning: Packages were dropped because memory wasn't pulled in time (input buffer)");
    } else {
      unsigned char *buffer = src->input.buffer;
      long           size   = readl(buffer, COM_BUFFERS_SIZE, from);

      type = readl(buffer, COM_BUFFERS_SIZE, from);

      if (size < 0) {
        void *content = (void*)readl(buffer, COM_BUFFERS_SIZE, from);
        memcpy(dest, content, size * sizeof(unsigned char));
        free(content);
      } else {
        size_t  stop1         = *from + size;
        size_t  stop2         = 0;
        size_t  size1         = size;
        if (stop1 >= COM_BUFFERS_SIZE) {
          stop2 = stop1 - COM_BUFFERS_SIZE;
          stop1 = 0;
          size1 = COM_BUFFERS_SIZE - *from;
        }
        memcpy(dest, &buffer[*from], size1 * sizeof(unsigned char));
        *from = stop1;
        if (stop2) {
          memcpy(dest, &buffer[0], stop2 * sizeof(unsigned char));
          *from = stop2;
        }
      }
      src->update -= size + 2 * sizeof(long);
      if (*from == src->input.index) src->update = 0;
    }
    pthread_mutex_unlock(&src->input.lock);
  }
  return type;
}

Interface *startservice(const short port, const int timeout, const int mask) {
  Interface *interface = malloc(sizeof(Interface));
  if (interface) {
    interface->port       = port;
    interface->timeout_ms = timeout;
    interface->mask       = htonl(mask);
    memset(interface->connections, 0, WS_MAX_CONN * sizeof(Connection*));
    pthread_create(&interface->serverthread, NULL, serve, (void*)interface);
  }
  return interface;
}

void stopservice(Interface *interface) {
  if (interface) {
    pthread_cancel(interface->serverthread);
    pthread_join(interface->serverthread, NULL);
    for (int i = 0; i < WS_MAX_CONN; i++) {
      if (interface->connections[i]) {
        pthread_join(interface->connections[i]->input.thread, NULL);
        free(interface->connections[i]);
        interface->connections[i] = NULL;
      }
    }
    free(interface);
  }
}