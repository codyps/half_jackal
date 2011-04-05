CC = gcc
RM = rm -f

TARGETS = ms pidk sizes us

all_SRC = frame_async.c term.c hj_print.c hj_send.c
obj = $(all_SRC:=.o)

.PHONY: all
all: build 

ms: maintain_speed.c.o
pidk: send_pid.c.o
sizes: sizes.c.o
us: unix.c.o

CFLAGS = -ggdb
override CFLAGS += -Wall -pipe
LDFLAGS = -Wl,--as-needed -O2

.PHONY: rebuild
rebuild: | clean build 

.PHONY: build
build: $(TARGETS)

%.c.o : %.c
	$(CC) $(CFLAGS) -MMD -c -o $@ $<

$(TARGETS) : $(obj) |
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	$(RM) $(TARGETS) *.d *.o

-include $(wildcard *.d)
