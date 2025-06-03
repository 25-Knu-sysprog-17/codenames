#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BOARD_SIZE 5
#define LEFT_PANEL_WIDTH 35
#define MAX_CARDS 25
#define RED_TEAM 0
#define BLUE_TEAM 1
#define RED_CARD 1
#define BLUE_CARD 2
#define NEUTRAL_CARD 3
#define ASSASSIN_CARD 4

typedef struct {
    char name[20];
    int type;
    int isUsed;
} Card;

typedef struct {
    Card cards[MAX_CARDS];
    char redTeamWords[9][20];
    char blueTeamWords[8][20];
    int redTeamScore;
    int blueTeamScore;
    int turn;
    int currentTries;
    int gameOver;
    char hintWord[20];
    int hintNumber;
} Game;

char answer_text[32] = "";

void draw_card(int y, int x, const char* word, int team, bool is_revealed, int is_Used) {
    int color_pair = is_revealed ? (team == 1 ? 2 : team == 2 ? 3 : team == 3 ? 4 : 1) : 1;
    attron(COLOR_PAIR(color_pair));
    mvprintw(y,     x, "+--------+");
    mvprintw(y + 1, x, "|        |");
    int padding = (8 - strlen(word)) / 2;
    if(!is_Used)    
        mvprintw(y + 1, x + 1 + padding, "%s", word);
    else
        mvprintw(y + 1, x + 1 + padding, "완료");
    mvprintw(y + 2, x, "+--------+");
    attroff(COLOR_PAIR(color_pair));
}

void draw_board(int offset_x, Game *game, bool reveal_all) {
    int start_y = 2;
    for (int y = 0; y < BOARD_SIZE; y++) {
        for (int x = 0; x < BOARD_SIZE; x++) {
            int index = y * BOARD_SIZE + x;
            int draw_y = start_y + y * 4;
            int draw_x = offset_x + x * 12;
            bool is_revealed = reveal_all || game->cards[index].isUsed;
            draw_card(draw_y, draw_x, game->cards[index].name, game->cards[index].type, is_revealed,game->cards[index].isUsed);
        }
    }
}

void draw_team_ui(int y, int x,int red,int blue) {
    attron(COLOR_PAIR(2));
    mvprintw(y, x, "빨강 팀      남은 단어 - %d",9-red);
    attroff(COLOR_PAIR(2));
    mvprintw(y + 1, x + 2, "bowons     신고");
    mvprintw(y + 2, x + 2, "suwon      신고");
    mvprintw(y + 3, x + 2, "eeee       신고");

    attron(COLOR_PAIR(3));
    mvprintw(y + 5, x, "파랑 팀      남은 단어 - %d",8-blue);
    attroff(COLOR_PAIR(3));
    mvprintw(y + 6, x + 2, "bowons     신고");
    mvprintw(y + 7, x + 2, "suwon      신고");
    mvprintw(y + 8, x + 2, "eeee       신고");
}

void draw_chat_ui(int y, int x, int height, int width) {
    mvaddch(y, x, ACS_ULCORNER);
    for (int i = 0; i < width - 2; ++i) addch(ACS_HLINE);
    addch(ACS_URCORNER);
    for (int i = 1; i < height - 1; ++i) {
        mvaddch(y + i, x, ACS_VLINE);
        mvaddch(y + i, x + width - 1, ACS_VLINE);
    }
    mvaddch(y + height - 1, x, ACS_LLCORNER);
    for (int i = 0; i < width - 2; ++i) addch(ACS_HLINE);
    addch(ACS_LRCORNER);
    mvprintw(y + 1, x + 2, "bowons: 뭐하냐");
    mvprintw(y + 2, x + 2, "빨간팀 - suwon: 개 아님?");
    mvprintw(y + 3, x + 2, "bowons: 야");
    mvaddch(y + height, x, ACS_ULCORNER);
    for (int i = 0; i < width - 12; ++i) addch(ACS_HLINE);
    addch(ACS_TTEE);
    for (int i = 0; i < 9; ++i) addch(ACS_HLINE);
    addch(ACS_URCORNER);
    mvprintw(y + height + 1, x + 1, "반칙아님?");
    mvprintw(y + height + 1, x + width - 10, "[제출]");

    mvaddch(y + height + 2, x, ACS_LLCORNER);
    for (int i = 0; i < width - 2; ++i) addch(ACS_HLINE);
    addch(ACS_LRCORNER);
}

void readWordsFromFile(char *filename, char wordList[MAX_CARDS][20]) {
    FILE *file = fopen(filename, "r");
    if (!file) exit(1);
    for (int i = 0; i < MAX_CARDS; i++) fscanf(file, "%s", wordList[i]);
    fclose(file);
}

void initGame(Game *game) {
    char wordList[MAX_CARDS][20];
    readWordsFromFile("words.txt", wordList);
    int used[MAX_CARDS] = {0};
    srand(time(NULL));
    for (int i = 0; i < MAX_CARDS; i++) {
        int r;
        do { r = rand() % MAX_CARDS; } while (used[r]);
        used[r] = 1;
        strcpy(game->cards[i].name, wordList[r]);
        game->cards[i].isUsed = 0;
        game->cards[i].type = (i < 9) ? RED_CARD : (i < 17) ? BLUE_CARD : (i < 24) ? NEUTRAL_CARD : ASSASSIN_CARD;
    }
    game->redTeamScore = 0; game->blueTeamScore = 0; game->turn = RED_TEAM;
    game->currentTries = 0; game->gameOver = 0; game->hintWord[0] = '\0'; game->hintNumber = 0;
}

void checkGameOver(Game *game) {
    if (game->redTeamScore == 9 || game->blueTeamScore == 8) {
        game->gameOver = 1;
    }
}

int main() {
    setlocale(LC_ALL, "");
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); start_color();
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_WHITE, COLOR_RED);
    init_pair(3, COLOR_WHITE, COLOR_BLUE);
    init_pair(4, COLOR_WHITE, COLOR_MAGENTA);

    Game game; initGame(&game);
    int board_offset_x = LEFT_PANEL_WIDTH + 2;
    int phase = 0;

    while (1) {
        clear();
        if (game.gameOver) 
        {
            mvprintw(0, 2, "게임 종료! 빨강팀 %d점, 파랑팀 %d점", game.redTeamScore, game.blueTeamScore);
            if(game.redTeamScore==9||((game.redTeamScore!=9)&&game.turn==RED_TEAM))
                mvprintw(3, 4, "레드팀 승리!");
            else
                mvprintw(3, 4, "블루팀 승리!");
            refresh(); getch(); break;
        }

        const char* team = (game.turn == RED_TEAM) ? "빨강팀" : "파랑팀";
        const char* role = (phase % 2 == 0) ? "팀장" : "팀원";
        mvprintw(0, 2, "[%s %s 차례]", team, role);

        draw_team_ui(2, 2, game.redTeamScore, game.blueTeamScore);
        draw_chat_ui(2, board_offset_x + 62, 14, 30);

        bool reveal_all = (phase % 2 == 0);
        draw_board(board_offset_x, &game, reveal_all);

        int y = 2 + BOARD_SIZE * 4;
        if (phase % 2 == 0) 
        {
            echo();
            mvprintw(y, board_offset_x, "힌트를 입력하세요: ");
            getnstr(game.hintWord, sizeof(game.hintWord) - 1);
            mvprintw(y + 1, board_offset_x, "연결 수를 입력하세요: ");
            char temp[4]; getnstr(temp, sizeof(temp) - 1);
            game.hintNumber = atoi(temp);
            game.currentTries = 0; noecho(); phase++;
        } else 
        {
            echo();
            mvprintw(y, board_offset_x, "정답: ");
            getnstr(answer_text, sizeof(answer_text) - 1);
            noecho();
            int found = 0;
            for (int i = 0; i < MAX_CARDS; i++) {
                if (!game.cards[i].isUsed && strcmp(game.cards[i].name, answer_text) == 0) 
                {
                    game.cards[i].isUsed = 1; found = 1;
                    int t = game.cards[i].type;
                    if (game.turn == RED_TEAM) 
                    {
                        if (t == RED_CARD) 
                        {
                            game.redTeamScore++;
                            game.currentTries++;
                        }
                        else 
                        {
                            if(t==BLUE_CARD)
                                game.blueTeamScore++; 
                            game.turn = BLUE_TEAM;
                            phase = 2;
                        }
                        if (t == ASSASSIN_CARD) 
                            game.gameOver = 1;
                    } else {
                        if (t == BLUE_CARD) 
                        {
                            game.blueTeamScore++;
                            game.currentTries++;
                        }
                        else 
                        {
                            game.turn = RED_TEAM;
                            phase = 0;
                        }
                        if (t == ASSASSIN_CARD) 
                            game.gameOver = 1;
                    }
                    draw_team_ui(2, 2,game.redTeamScore,game.blueTeamScore);
                    checkGameOver(&game);
                    if (game.gameOver) break;
                    if (game.currentTries >= game.hintNumber + 1) {
                        game.turn = (game.turn == RED_TEAM) ? BLUE_TEAM : RED_TEAM;
                        phase = (game.turn == RED_TEAM) ? 0 : 2;
                    }
                    break;
                }
            }
            if (!found) {
                mvprintw(y + 1, board_offset_x, "단어를 찾을 수 없습니다.");
                getch();
            }
        }

        refresh();
    }

    endwin();
    return 0;
}
