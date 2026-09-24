UNAME_S := $(shell uname -s)

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -O2
LDFLAGS = -lm

SRCS    = src/main.c src/lexer.c src/parser.c src/ast.c \
          src/chunk.c src/value.c src/compiler.c src/vm.c src/debug.c src/report.c \
          src/native_net.c src/native_fs.c src/native_os.c
OBJS    = $(SRCS:.c=.o)

ifeq ($(UNAME_S),Darwin)
  CFLAGS  += -DMYC_HAS_GRAPHICS=1
  GFX_OBJ  = src/native_graphics.o
  LDFLAGS += -framework AppKit -framework Foundation
else
  GFX_OBJ  = src/native_graphics_stub.o
endif

.PHONY: all clean test

all: myc

myc: $(OBJS) $(GFX_OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(GFX_OBJ) $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

src/native_graphics.o: src/native_graphics.m src/native_graphics.h
	$(CC) -Wall -Wextra -O2 -fobjc-arc -DMYC_HAS_GRAPHICS=1 -c -o $@ $<

src/native_graphics_stub.o: src/native_graphics_stub.c src/native_graphics.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f myc $(OBJS) src/native_graphics.o src/native_graphics_stub.o

test: myc
	./tests/run_tests.sh
