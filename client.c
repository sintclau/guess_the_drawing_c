// client.c
#include <stdio.h>
#include <string.h>
#include <ncurses.h>

#define MAX_WIDTH 40
#define MAX_HEIGHT 120

typedef struct {
    int x;
    int y;
    int tool_type;
}user_cursor;

void screen(char *username, int toolType) {
    move(0,0);

    if(toolType == 0) {
        printw("Guess the Drawing -- Username: %s -- Selected tool: Eraser", username);
    } else if(toolType == 1) {
        printw("Guess the Drawing -- Username: %s -- Selected tool: Pencil", username);
    }

    for(int i = 1; i < MAX_WIDTH; i++) {
        for(int j = 0; j < MAX_HEIGHT; j++) {
            if((i != 1 || i != (MAX_WIDTH-1)) && (j == 0 || j == (MAX_HEIGHT-1))) {
                move(i, j);
                printw("=");
            } else if(i == 1 || i == (MAX_WIDTH-1)) {
                move(i, j);
                printw("=");
            }
        }
    }
}

void game(char *username) {
    // init screen and sets up screen
    initscr();
    keypad(stdscr, TRUE); // enable arrow keys
    noecho(); // dont display typed chars
    curs_set(0); // hide cursor
    
    // init user cursor locaation
    user_cursor cursor = {MAX_WIDTH/2, MAX_HEIGHT/2, 1};

    // initial screen print
    screen(username, cursor.tool_type);
    refresh();

    // loop until 'q' is pressed
    int ch;
    while((ch = getch()) != 'q') {
        if(cursor.tool_type == 1) {
            mvprintw(cursor.x, cursor.y, "O");
        } else if(cursor.tool_type == 0) {
            mvprintw(cursor.x, cursor.y, " ");
        }

        // interpret user input
        switch (ch) {
            case KEY_UP:
                cursor.x = (cursor.x > 2) ? cursor.x - 1 : cursor.x;
                break;
            case KEY_DOWN:
                cursor.x = (cursor.x < MAX_WIDTH - 2) ? cursor.x + 1 : cursor.x;
                break;
            case KEY_LEFT:
                cursor.y = (cursor.y > 1) ? cursor.y - 1 : cursor.y;
                break;
            case KEY_RIGHT:
                cursor.y = (cursor.y < MAX_HEIGHT - 2) ? cursor.y + 1 : cursor.y;
                break;
            case 't':
                if(cursor.tool_type == 1) {
                    cursor.tool_type = 0;
                } else if(cursor.tool_type == 0) {
                    cursor.tool_type = 1;
                } else {
                    cursor.tool_type = 1;
                }
                break;
        }

        // show user cursor position
        mvprintw(cursor.x, cursor.y, "X");

        screen(username, cursor.tool_type);
        refresh();
    }

    // deallocates memory and ends ncurses
    endwin();
}

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("Usage: %s <username> <server_ip>\n", argv[0]);
        return 1;
    }

    int a, b, c, d;
    if(sscanf(argv[2], "%d.%d.%d.%d", &a, &b, &c, &d) != 4 || a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) {
        if(strcmp(argv[2], "localhost") != 0) {
            printf("Invalid server IP format\n");
            return 1;
        }
    }

    game(argv[1]);

    return 0;
}