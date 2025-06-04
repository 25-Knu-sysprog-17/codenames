#ifndef CLIENT_H
#define CLIENT_H

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <openssl/ssl.h>

#define SERVER_IP "127.0.0.1"
#define SSL_PORT 55014
#define TCP_PORT 55015
#define CLIENT_MAX_INPUT 128
#define TOKEN_LEN 64

extern SSL *ssl;
extern char token[TOKEN_LEN];
extern int game_sock;

typedef enum {
    SCENE_LOGIN,     // 로그인 화면
    SCENE_SIGNUP,    // 회원가입 화면
    SCENE_MAIN,      // 메인 화면(게임)
    SCENE_INVALID_TOKEN,
    SCENE_ERROR,
    SCENE_RESULT,
    SCENE_EXIT       // 종료
} SceneState;

#endif
