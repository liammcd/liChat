TARGETS = chatserver chatclient
SRCS = chatserver.c chatclient.c

.PHONY: clean default all

default: all
all: $(TARGETS)

build: $(TARGETS)

chatserver: chatserver.o
	$(CC) $^ -o $@ 

chatclient: chatclient.o
	$(CC) $^ -o $@ -lncurses -lpthread

clean:
	rm -f $(TARGETS) chatserver.o chatclient.o
