#ifndef WEBSOCKETYPES_HPP
#define WEBSOCKETYPES_HPP

#include <wsserver.h>
#include <cstddef>

namespace ws {
  enum DataType {
    DATA_PING         = READ_PING_TIME,
    DATA_TEXT         = READ_TEXT,
    DATA_BINARY       = READ_BINARY,
    DATA_FAILURE      = READ_FAILURE,
    DATA_INCOMPLETE   = READ_BUFFER_OVERFLOW,
    DATA_CLOSE_CLIENT = READ_CONNECTION_CLOSED_CLIENT,
    DATA_CLOSE_SERVER = READ_CONNECTION_CLOSED_SERVER
  };

  struct RawData {
    unsigned char buffer[FRAME_MAX_FRAGMENTS * FRAME_MAX_SIZE];
    size_t        size;
    DataType      type;
  };
}

#endif