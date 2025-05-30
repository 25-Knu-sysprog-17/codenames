CC = gcc
CFLAGS = -Wall -I../gui -I../client
LDFLAGS = -lncurses -lssl -lcrypto

VPATH = ../gui

TARGET = client

SRCS = client.c login_screen.c signup_screen.c gui_utils.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)
