CC     = gcc
CXX    = g++
CFLAGS = -Iinc -Wall -lcrypto -pthread
VPATH  = src/

LIBRARIES = lib/
BINARIES  = bin/
OBJECTS   = obj/
BASE_SRC  = http.c wsserver.c

ifeq "$(LANG)" "C++"
	CMP      = $(CXX)
	MAIN     = test/main.cpp
	LANG_SRC = websocket.cpp wsconnection.cpp
	TARGET   = test_cpp
	LIB      = libcppws.a
else
	CMP      = $(CC)
	MAIN     = test/main.c
	LANG_SRC = websocket.c
	TARGET   = test_c
	LIB      = libcws.a
endif

OBJ_LIST  = $(addprefix $(OBJECTS), $(BASE_SRC:=.o) $(LANG_SRC:=.o))

.PHONY: test clean clean-lib clean-obj clean-bin lib bin obj

$(OBJECTS)%.cpp.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) 

$(OBJECTS)%.c.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(LIBRARIES)$(LIB): $(OBJ_LIST)
	ar rcs $@ $^

lib: $(LIBRARIES)$(LIB)

clean-bin:
	rm -f bin/*

clean-obj:
	rm -f obj/*.o

clean-lib:
	rm -f lib/*.a

clean: clean-lib clean-obj clean-bin

test: clean $(eval CFLAGS+=-g) $(OBJ_LIST)
	$(CMP) -o $(BINARIES)$(TARGET) $(MAIN) $(OBJ_LIST) $(CFLAGS)