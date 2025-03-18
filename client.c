#include <stdio.h>
#include <string.h>
#include <ncurses.h>

#define MAX_WIDTH 40
#define MAX_HEIGHT 120

typedef struct {
    int gameMode; // 0 - lobby, 1 - guessing, 2 - drawing
    char *word;
} gameDataS;

typedef struct {
    int x;
    int y;
    int tool_type;
} user_cursor;

typedef struct {
    char *username;
    int points;
    int ready;
} user;

void drawingScreen(user *myUser, user **userList, int userListSize, int toolType) {
    move(0, 0);
    printw("Guess the Drawing -- Username: %s -- Points: %d -- Selected tool: %s", 
           myUser->username, myUser->points, toolType ? "Pencil" : "Eraser");
    
    for (int i = 1; i < MAX_WIDTH; i++) {
        for (int j = 0; j < MAX_HEIGHT; j++) {
            if ((i != 1 || i != (MAX_WIDTH - 1)) && (j == 0 || j == (MAX_HEIGHT - 1))) {
                mvprintw(i, j, "=");
            } else if (i == 1 || i == (MAX_WIDTH - 1)) {
                mvprintw(i, j, "=");
            }
        }
    }
}

void lobbyScreen(user *myUser, user **userList, int userListSize) {
    move(0, 0);
    printw("Guess the Drawing -- Lobby\n\n");
    for (int i = 0; i < userListSize; i++) {
        printw("%s - %d points - %s\n", 
               userList[i]->username, userList[i]->points, 
               userList[i]->ready ? "READY" : "UNREADY");
    }
}

void game(user *myUser, user **userList, int *userListSize, gameDataS *gameData) {
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);
    user_cursor cursor = {MAX_WIDTH / 2, MAX_HEIGHT / 2, 1};
    
    switch (gameData->gameMode) {
        case 0:
            lobbyScreen(myUser, userList, *userListSize);
            break;
        case 2:
            drawingScreen(myUser, userList, *userListSize, cursor.tool_type);
            break;
    }
    
    refresh();
    int ch;
    while ((ch = getch()) != 'q') {
        switch (gameData->gameMode) {
            case 0:
                if (ch == ' ') {
                    myUser->ready = !myUser->ready;
                    userList[0]->ready = myUser->ready;
                }
                lobbyScreen(myUser, userList, *userListSize);
                break;
            case 2:
                if (cursor.tool_type == 1) {
                    mvprintw(cursor.x, cursor.y, "O");
                } else {
                    mvprintw(cursor.x, cursor.y, " ");
                }

                switch (ch) {
                    case KEY_UP: cursor.x = (cursor.x > 2) ? cursor.x - 1 : cursor.x; break;
                    case KEY_DOWN: cursor.x = (cursor.x < MAX_WIDTH - 2) ? cursor.x + 1 : cursor.x; break;
                    case KEY_LEFT: cursor.y = (cursor.y > 1) ? cursor.y - 1 : cursor.y; break;
                    case KEY_RIGHT: cursor.y = (cursor.y < MAX_HEIGHT - 2) ? cursor.y + 1 : cursor.y; break;
                    case 't': cursor.tool_type = !cursor.tool_type; break;
                }
                mvprintw(cursor.x, cursor.y, "X");
                drawingScreen(myUser, userList, *userListSize, cursor.tool_type);
                break;
        }
        refresh();
    }
    endwin();
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <username> <server_ip>\n", argv[0]);
        return 1;
    }
    
    int a, b, c, d;
    if (sscanf(argv[2], "%d.%d.%d.%d", &a, &b, &c, &d) != 4 || a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) {
        if (strcmp(argv[2], "localhost") != 0) {
            printf("Invalid server IP format\n");
            return 1;
        }
    }
    
    user myUser = {argv[1], 0, 0};
    user *userList[] = {&myUser, &(user){"test", 0, 0}};
    int userListSize = 2;
    gameDataS gameData = {0, "test"};
    
    game(&myUser, userList, &userListSize, &gameData);
    return 0;
}
