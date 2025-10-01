CC      = gcc
CFLAGS  = -O2 -Wall

SRC       = pingpong.c

PIPES     = ping-00 ping-01 ping

.PHONY: all 

all: $(PIPES)

ping-00: pingpong.c
	$(CC) $(CFLAGS) pingpong.c -o $@

ping-01: pingpong.c
	$(CC) $(CFLAGS) pingpong.c -o $@ -DPARENT_CPU=0 -DCHILD_CPU=1

ping: pingpong.c
	$(CC) $(CFLAGS) pingpong.c -o $@ -DUSE_AFFINITY=0

clean:
	rm -f $(PIPES) 
