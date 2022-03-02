CMP    = gcc
MAIN   = main.c
CFLAGS = -Iinc -Wall
VPATH  = src/

LIBRARIES = lib/
BINARIES  = bin/
OBJECTS   = obj/
BASE_SRC  = http.c wsserver.c
BASE_OBJ  = $(addprefix $(OBJECTS), $(BASE_SRC:.c=.o))

ifeq "$(LANG)" "c++"
	CMP  = g++
	MAIN = main.cpp
endif

.PHONY: test clean


$(OBJECTS)%.o: %.c
	$(CMP) -c -o $@ $< $(CFLAGS) 

test: $(BASE_OBJ)
	$(eval CFLAGS+=-g)
	$(CMP) -c -o bin/test test/$(MAIN) $(BASE_OBJ) $(CFLAGS)

clean:
	rm obj/*.o