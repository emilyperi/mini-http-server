CC=gcc
CFLAGS=-g -ggdb3 -Wall -Wextra -std=gnu99
LDFLAGS=-pthread
EXECUTABLES=httpserver poolserver
SOURCE=httpserver.c libhttp.c wq.c

all: $(EXECUTABLES)

httpserver: $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) -D BASICSERVER $(SOURCE) -o $@
poolserver: $(SOURCE)
	$(CC) $(CFLAGS) $(LDFLAGS) -D POOLSERVER $(SOURCE) -o $@

clean:
	rm -f $(EXECUTABLES)
