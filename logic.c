#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define MAX_CARDS 25
#define RED_TEAM 0
#define BLUE_TEAM 1

// 카드 종류
#define RED_CARD 1
#define BLUE_CARD 2
#define NEUTRAL_CARD 3
#define ASSASSIN_CARD 4

// 카드 구조체
typedef struct {
    char name[20];
    int type; // 1: red, 2: blue, 3: neutral, 4: assassin
    int isUsed; // 0: not used, 1: used
} Card;

// 게임 상태 구조체
typedef struct {
    Card cards[MAX_CARDS];
    char redTeamWords[9][20];
    char blueTeamWords[8][20];
    int redTeamScore;
    int blueTeamScore;
    int turn; // 0: red's turn, 1: blue's turn
    int currentTries;
    int gameOver;
    char hintWord[20]; // 힌트 단어
    int hintNumber; // 힌트와 관련된 카드 수
} Game;

// 카드 리스트 읽기 함수
void readWordsFromFile(char *filename, char words[MAX_CARDS][20]) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error: could not open file %s\n", filename);
        exit(1);
    }

    for (int i = 0; i < MAX_CARDS; i++) {
        fscanf(file, "%s", words[i]);
    }

    fclose(file);
}

// 카드 초기화
void initGame(Game *game) {
    char wordList[MAX_CARDS][20];
    readWordsFromFile("words.txt", wordList);

    int used[MAX_CARDS] = {0};
    srand(time(NULL));

    // 카드 배치 (단어와 종류)
    for (int i = 0; i < MAX_CARDS; i++) {
        int randomIndex;
        do {
            randomIndex = rand() % MAX_CARDS;
        } while (used[randomIndex]);
        used[randomIndex] = 1;
        strcpy(game->cards[i].name, wordList[randomIndex]);
        game->cards[i].isUsed = 0;
    }

    // 팀별 단어 배분 (임의로 배정)
    int redCount = 0, blueCount = 0;
    for (int i = 0; i < MAX_CARDS; i++) {
        if (redCount < 9) {
            strcpy(game->redTeamWords[redCount], game->cards[i].name);
            redCount++;
        } else if (blueCount < 8) {
            strcpy(game->blueTeamWords[blueCount], game->cards[i].name);
            blueCount++;
        }
    }

    // 카드 종류 설정
    for (int i = 0; i < MAX_CARDS; i++) {
        if (i < 9) {
            game->cards[i].type = RED_CARD;   // 빨간팀 카드
        } else if (i < 17) {
            game->cards[i].type = BLUE_CARD;  // 파란팀 카드
        } else if (i < 24) {
            game->cards[i].type = NEUTRAL_CARD; // 일반 카드
        } else {
            game->cards[i].type = ASSASSIN_CARD; // 암살자 카드
        }
    }

    game->redTeamScore = 0;
    game->blueTeamScore = 0;
    game->turn = RED_TEAM;
    game->currentTries = 0;
    game->gameOver = 0;
    game->hintWord[0] = '\0'; // 초기화
    game->hintNumber = 0; // 초기화
}
// 차례 종료 함수
int checkTry(Game *game) {
    // 추리 횟수가 힌트에서 지정된 횟수를 넘기면 차례를 넘김
    if (game->currentTries >= game->hintNumber+1) {
        return 0;
    }
    return 1;
}

// 카드 출력 함수 (5x5 테이블 형식)
void printCardTable(Game *game) {
    printf("\nCard Table (5x5):\n");
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            int idx = i * 5 + j;
            if (game->cards[idx].isUsed == 0) {
                // 카드의 이름과 타입을 출력
                char *cardType = "";
                switch (game->cards[idx].type) {
                    case RED_CARD:
                        cardType = "Red";
                        break;
                    case BLUE_CARD:
                        cardType = "Blue";
                        break;
                    case NEUTRAL_CARD:
                        cardType = "Neutral";
                        break;
                    case ASSASSIN_CARD:
                        cardType = "Assassin";
                        break;
                }
                printf("%s(%s) ", game->cards[idx].name, cardType);
            } else {
                printf("USED       ");  // 이미 사용된 카드는 'USED'로 표시
            }
        }
        printf("\n");
    }
}

// 힌트 제공
void giveHint(Game *game) {
    if (game->gameOver) {
        printf("Game Over!\n");
        return;
    }

    printf("%s team's turn!\n", game->turn == RED_TEAM ? "Red" : "Blue");
    printf("Enter your hint word and number: ");
    scanf("%s", game->hintWord);
    scanf("%d", &game->hintNumber);

    printf("Team Leader gives the hint: %s, %d\n", game->hintWord, game->hintNumber);
}

// 추리 처리
void makeGuess(Game *game) {
    if (game->gameOver) {
        printf("Game Over!\n");
        return;
    }

    char guess[20];
    printf("Enter your guess: ");
    scanf("%s", guess);

    int correctGuess = 0;
    for (int i = 0; i < MAX_CARDS; i++) {
        if (strcmp(game->cards[i].name, guess) == 0 && !game->cards[i].isUsed) {
            correctGuess = 1;
            game->cards[i].isUsed = 1;  // 해당 카드를 사용 처리

            // 카드 종류에 따른 처리
            if (game->turn == RED_TEAM) {
                if (game->cards[i].type == RED_CARD) {
                    printf("Correct guess for Red team!\n");
                    game->currentTries++;
                    game->redTeamScore++;
                    if(checkTry(game))
                    {
                        makeGuess(game);
                    }


                } else if (game->cards[i].type == BLUE_CARD) {
                    printf("Incorrect! Blue team gets a point.\n");
                    game->blueTeamScore++;
                } else if (game->cards[i].type == NEUTRAL_CARD) {
                    printf("Neutral card! The turn ends.\n");
                } else if (game->cards[i].type == ASSASSIN_CARD) {
                    printf("Assassin card! Red team loses!\n");
                    game->gameOver = 1;
                    return;
                }
                game->turn =1;
            } else {
                if (game->cards[i].type == BLUE_CARD) {
                    printf("Correct guess for Blue team!\n");
                    game->blueTeamScore++;
                    if(checkTry(game))
                    {
                        makeGuess(game);
                    }
                } else if (game->cards[i].type == RED_CARD) {
                    printf("Incorrect! Red team gets a point.\n");
                    game->redTeamScore++;
                } else if (game->cards[i].type == NEUTRAL_CARD) {
                    printf("Neutral card! The turn ends.\n");
                } else if (game->cards[i].type == ASSASSIN_CARD) {
                    printf("Assassin card! Blue team loses!\n");
                    game->gameOver = 1;
                    return;
                }
                game->turn =0;
            }

            // 추리 횟수 처리
            game->currentTries++;

            // 차례 끝내기

            return;
        }
    }

    printf("Card not found, please guess again.\n");
    makeGuess(game);
    
}



int main() {
    Game game;
    initGame(&game);

    // 카드 테이블을 5x5 형식으로 출력
    printCardTable(&game);

    while (!game.gameOver) {
        giveHint(&game);
        makeGuess(&game);

        // 점수 출력
        printf("Red team score: %d, Blue team score: %d\n", game.redTeamScore, game.blueTeamScore);
    }

    return 0;
}
