CC = gcc
CFLAGS = -Wall -g -I../gui -I../client
LDFLAGS = -lncurses -lssl -lcrypto

VPATH = ../gui

TARGET = client

SRCS = client.c login_screen.c signup_screen.c gui_utils.c main_screen.c result_screen.c gui_invalid_token.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)
