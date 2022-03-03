CC     = gcc
CXX    = g++
CFLAGS = -Iinc -Wall -lcrypto -pthread
VPATH  = src/

LIBRARIES = lib/
BINARIES  = bin/
OBJECTS   = obj/
BASE_SRC  = http.c wsserver.c

ifeq "$(LANG)" "c++"
	CMP      = $(CXX)
	MAIN     = test/main.cpp
	LANG_SRC = websocket.cpp wsconnection.cpp
	TARGET   = test_cpp
else
	CMP      = $(CC)
	MAIN     = test/main.c
	LANG_SRC = websocket.c
	TARGET   = test_c
endif

OBJ_LIST  = $(addprefix $(OBJECTS), $(BASE_SRC:=.o) $(LANG_SRC:=.o))

.PHONY: test clean

$(OBJECTS)%.cpp.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) 

$(OBJECTS)%.c.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 

test: $(OBJ_LIST)
	$(eval CFLAGS+=-g)
	$(CMP) -o $(BINARIES)/$(TARGET) $(MAIN) $(OBJ_LIST) $(CFLAGS)

clean:
	rm obj/*.o