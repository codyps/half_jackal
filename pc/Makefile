CC = gcc
RM = rm -f

TARGETS=ms pidk

.SECONDEXPANSION:
all_SRC = frame_async.c term.c hj_print.c hj_send.c
ms_SRC = maintain_speed.c $(all_SRC)
pidk_SRC = send_pid.c $(all_SRC)

ms:   SRC = $(ms_SRC)
pidk: SRC = $(pidk_SRC)

CFLAGS = -ggdb
override CFLAGS += -Wall -pipe

.PHONY: all
all: build ;

.PHONY: rebuild
rebuild: | clean build ;

.PHONY: build
build: $(TARGETS) ;

$(TARGETS): $$($$@_SRC)
	$(CC) $(CFLAGS) -MMD -o $@ $^

.PHONY: clean
clean:
	$(RM) $(TARGETS) *.d

-include $(wildcard *.d)
