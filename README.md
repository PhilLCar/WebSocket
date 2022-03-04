# WebSocket
Minimal WebSocket Server ([RFC6455 Protocol](https://datatracker.ietf.org/doc/html/rfc6455)) implementation for C/C++.

The goal of this implementation is to provide a minimal but efficient and easy to use solution for personnal web servers that need near instantaneous communication with a program. The main use-case is to make a web-based interface for any C/C++ program.

# How to use
The project is built using a Makefile.

## Build
### C
To build the C library simply run:
``` $ make lib ```

In the C project include:
```C
#include <websocket.h>
```
And compile using the flag `-lcws` (make sure ld can detect `libcws.a`).

### C++
To build the C++ library run:
``` $ make lib LANG=C++ ```

In the C project include:
```C++
#include <websocket.hpp>
```
And compile using the flag `-lcppws` (make sure ld can detect `libcppws.a`).

## Test
A very simple WebSocket server is available in the test folder, to serve both as a test and a demo.

### C
To build the C test simply run:
``` $ make test ```

Then run:
``` $ ./bin/test_c ```

Open `test.html` and attempt to communicate with the server.

### C++
To build the C test simply run:
``` $ make test LANG=C++ ```

Then run:
``` $ ./bin/test_cpp ```

Open `test.html` and attempt to communicate with the server.