# 컴파일러 및 옵션
CC = gcc
CFLAGS = -Wall -I../server -I../src
LIBS = -lpthread -lsqlite3 -lssl -lcrypto  # SHA256 등 OpenSSL 사용 시 필요

# 소스 파일
VPATH = ../db ../server ../src
SRCS = server.c user_auth.c handler.c session.c room_manager.c game_result.c user_info.c user_report.c game_result.c
OBJS = $(SRCS:.c=.o)

# 최종 실행 파일 이름
TARGET = server

# 최종 빌드
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

# 개별 .c 파일 빌드
%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

# 정리
clean:
	rm -f $(TARGET) $(OBJS)
