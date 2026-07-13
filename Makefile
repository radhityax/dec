CC = cc
CFLAGS = -std=c11 -Os -s -DNDEBUG
CFLAGS += -flto -Wall -Wextra -Wpedantic -pthread
LDFLAGS = -s -pthread 
LIBS = -lcmark

.PHONY: all clean debug	

all: dec

dec: main.o builder.o server.o util.o toml.o template.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f *.o dec

%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

debug: CFLAGS = -std=c11 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Wpedantic
debug: LDFLAGS = -fsanitize=address,undefined
debug: dec

