#include "gui_utils.h"
#include <ncurses.h>

const char* art[] = {
    ":::::::  :::::::  :::::    :::::::      ::   ::    ::     ::: :::  :::::::  :::::::",
    "+:       +:   :+  +:  :+   +:           +:+  :+   +: :+   +:: :+:  +:       +:     ",
    "#+       #+   +#  #+   +#  #+#+#+#      #+#+ +#  #+#+#+#  #+ + #+  #+#+#+#  #+#+#+#",
    "#+       #+   +#  #+  +#   #+           #+ +#+#  #+   +#  #+   #+  #+            +#",
    "#######  #######  #####    ####***      ##   ##  ##   ##  ##   ##  #######  #######"
};

const int art_lines = 5;

// 테두리 그리기
void draw_border() {
    int max_y, max_x; 
    getmaxyx(stdscr, max_y, max_x);
    for (int i = 0; i < max_x; i++) {
        mvaddch(0, i, '-');
        mvaddch(max_y-1, i, '-');
    }
    for (int i = 1; i < max_y-1; i++) {
        mvaddch(i, 0, '|');
        mvaddch(i, max_x-1, '|');
    }
    mvaddch(0, 0, '+');
    mvaddch(0, max_x-1, '+');
    mvaddch(max_y-1, 0, '+');
    mvaddch(max_y-1, max_x-1, '+');
}