CC = gcc
RM = rm -f

TARGET = hdlc_frame

SRC = main.c frame_async.c term.c hj_print.c hj_send.c

CFLAGS = -ggdb
override CFLAGS += -Wall -pipe

all: build

rebuild: | clean build

build: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -MMD -o $(TARGET) $(SRC)

clean:
	$(RM) $(TARGET) *.d

-include $(wildcard *.d)
