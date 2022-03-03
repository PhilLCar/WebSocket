CC     = gcc
CPPC   = g++
CFLAGS = -Iinc -Wall
VPATH  = src/

LIBRARIES = lib/
BINARIES  = bin/
OBJECTS   = obj/
BASE_SRC  = http.c wsserver.c

ifeq "$(LANG)" "c++"
	CMP      = $(CPPC)
	MAIN     = main.cpp
	LANG_SRC = websocket.cpp wsconnection.cpp
else
	CMP      = $(CC)
	MAIN     = main.c
	LANG_SRC = websocket.c
endif

OBJ_LIST  = $(addprefix $(OBJECTS), $(BASE_SRC:=.o) $(LANG_OBJ:=.o))

.PHONY: test clean

$(OBJECTS)%.cpp.o: %.cpp
	$(CPPC) -c -o $@ $< $(CFLAGS) 

$(OBJECTS)%.c.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) 

test: $(OBJ_LIST)
	$(eval CFLAGS+=-g)
	$(CMP) -c -o bin/test test/$(MAIN) $(OBJ_LIST) $(CFLAGS)

clean:
	rm obj/*.o