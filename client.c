#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>

#define MAX_WIDTH 40
#define MAX_HEIGHT 120
#define MAX_CLIENTS 10
#define PORT 9000

typedef struct {
    int gameMode; // 0 - lobby, 1 - guessing, 2 - drawing
    char word[20];
    char drawing[MAX_WIDTH-1][MAX_HEIGHT-1];
} gameDataS;

typedef struct {
    int x;
    int y;
    int tool_type;
} user_cursor;

typedef struct {
    char username[50];
    int points;
    int ready;
} user;

gameDataS gameData;
user myUser;
user_cursor cursor = {MAX_WIDTH / 2, MAX_HEIGHT / 2, 1};
user userList[MAX_CLIENTS];
int sockfd;

void clearDrawing() {
    for (int i = 0; i < MAX_WIDTH-2; i++) {
        for (int j = 0; j < MAX_HEIGHT-2; j++) {
            gameData.drawing[i][j] = ' ';  // Clear inside the borders
        }
    }
}

void drawingScreen() {
    move(0, 0);
    printw("Guess the Drawing -- Username: %s -- Points: %d -- Selected tool: %s", 
           myUser.username, myUser.points, cursor.tool_type ? "Pencil" : "Eraser");

    for (int i = 1; i < MAX_WIDTH; i++) {
        for (int j = 0; j < MAX_HEIGHT; j++) {
            if (i == 1 || i == MAX_WIDTH-1 || j == 0 || j == MAX_HEIGHT-1) {
                mvprintw(i, j, "=");  // Draw border
            } else {
                mvprintw(i, j, "%c", gameData.drawing[i][j]);  // Display drawing inside
            }
        }
    }
}


void guessingScreen() {
    move(0, 0);
    printw("Guess the Drawing -- Username: %s -- Points: %d", 
           myUser.username, myUser.points);
}

void lobbyScreen() {
    move(0, 0);
    printw("Guess the Drawing -- Lobby\n\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        printw("%s - %d points - %s\n", 
               userList[i].username, userList[i].points, 
               userList[i].ready ? "READY" : "UNREADY");
    }
}

void game() {
    // init screen and sets up screen
    initscr();
    keypad(stdscr, TRUE); // enable arrow keys
    noecho(); // dont display typed chars
    curs_set(0); // hide cursor
    
    switch (gameData.gameMode) {
        case 0:
            lobbyScreen();
            break;
        case 1:
            guessingScreen();
            break;
        case 2:
            clearDrawing();
            drawingScreen();
            break;
    }
    
    refresh();
    int ch;
    while ((ch = getch()) != 'q') {
        switch (gameData.gameMode) {
            case 0:
                if (ch == ' ') {
                    myUser.ready = !myUser.ready;
                    userList[0].ready = myUser.ready;
                }
                lobbyScreen();
                break;
            case 1:
                guessingScreen();
                break;
            case 2:
                if (cursor.tool_type == 1) {
                    mvprintw(cursor.x, cursor.y, "O");
                    gameData.drawing[cursor.x][cursor.y] = 'O';
                } else {
                    mvprintw(cursor.x, cursor.y, " ");
                    gameData.drawing[cursor.x][cursor.y] = ' ';
                }

                switch (ch) {
                    case KEY_UP: cursor.x = (cursor.x > 2) ? cursor.x - 1 : cursor.x; break;
                    case KEY_DOWN: cursor.x = (cursor.x < MAX_WIDTH - 2) ? cursor.x + 1 : cursor.x; break;
                    case KEY_LEFT: cursor.y = (cursor.y > 1) ? cursor.y - 1 : cursor.y; break;
                    case KEY_RIGHT: cursor.y = (cursor.y < MAX_HEIGHT - 2) ? cursor.y + 1 : cursor.y; break;
                    case 't': cursor.tool_type = !cursor.tool_type; break;
                }

                // show user cursor position
                mvprintw(cursor.x, cursor.y, "X");
                gameData.drawing[cursor.x][cursor.y] = 'X';

                drawingScreen();
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
    
    // SERVER CONNECTION
    struct sockaddr_in serverAddr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[2], &serverAddr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    // GAME
    strcpy(myUser.username, argv[1]);
    myUser.ready = 0;
    myUser.points = 0;
    gameData.gameMode = 2;
    strcpy(gameData.word, "test");
    
    game();
    close(sockfd);
    
    return 0;
}
