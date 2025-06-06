CC = gcc
CFLAGS = -Wall -g -I../gui -I../client
LDFLAGS = -lncursesw -lssl -lcrypto

VPATH = ../gui

TARGET = client

SRCS = client.c login_screen.c signup_screen.c gui_utils.c lobby_screen.c result_screen.c gui_invalid_token.c connect_to_server.c waiting_screen.c codenames_screen.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)
