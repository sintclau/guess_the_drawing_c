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

#define TYPE_GAME_DATA 1
#define TYPE_USER_LIST 2
#define TYPE_PIXEL_UPDATE 3

typedef struct {
    int gameMode;
    char word[20];
    char drawing[MAX_WIDTH][MAX_HEIGHT];
    int drawingUser;
} gameDataS;

typedef struct {
    int id;
    char username[50];
    int points;
    int ready;
} user;

typedef struct {
    int x;
    int y;
    int tool_type;
} user_cursor;

typedef struct {
    int x, y;
    char value;
} PixelUpdate;

gameDataS gameData;
user myUser;
user_cursor cursor = {MAX_WIDTH / 2, MAX_HEIGHT / 2, 1};
user userList[MAX_CLIENTS];
int sockfd;

// ========= HELPER FUNCTION ==========
int recv_all(int socket, void *buffer, size_t length) {
    size_t total = 0;
    while (total < length) {
        ssize_t bytes = recv(socket, (char*)buffer + total, length - total, 0);
        if (bytes <= 0) return -1;
        total += bytes;
    }
    return 0;
}

void updateScreen() {
    erase(); 
    if (gameData.gameMode == 0) {
        printw("Guess the Drawing -- Lobby\n\n");
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (strcmp(userList[i].username, "") != 0)
                printw("%d - %s - %d points - %s\n", userList[i].id, userList[i].username,
                       userList[i].points, userList[i].ready ? "READY" : "UNREADY");
        }
    } else {
        printw("Guess the Drawing -- Username: %s -- Points: %d\n", myUser.username, myUser.points);
        for (int i = 0; i < MAX_WIDTH; i++) {
            for (int j = 0; j < MAX_HEIGHT; j++) {
                if (gameData.drawing[i][j] != 0)
                    mvprintw(i, j, "%c", gameData.drawing[i][j]);
            }
        }
        mvprintw(cursor.x, cursor.y, "X");
    }
    refresh();
}

void gameLoop() {
    initscr();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);
    noecho();
    curs_set(0);

    while (1) {
        char type;
        ssize_t readBytes = recv(sockfd, &type, 1, MSG_DONTWAIT);
        if (readBytes > 0) {
            if (type == TYPE_GAME_DATA) {
                if (recv_all(sockfd, &gameData, sizeof(gameDataS)) == 0) {
                    updateScreen();
                }
            } else if (type == TYPE_USER_LIST) {
                if (recv_all(sockfd, userList, sizeof(userList)) == 0) {
                    updateScreen();
                }
            } else if (type == TYPE_PIXEL_UPDATE) {
                PixelUpdate p;
                if (recv_all(sockfd, &p, sizeof(PixelUpdate)) == 0) {
                    if (p.x >= 0 && p.x < MAX_WIDTH && p.y >= 0 && p.y < MAX_HEIGHT) {
                        gameData.drawing[p.x][p.y] = p.value;
                        updateScreen();
                    }
                }
            }
        }

        int ch = getch();
        if (ch == 'q') break;

        if (gameData.gameMode == 0 && ch == ' ') {
            myUser.ready = !myUser.ready;
            send(sockfd, &myUser.ready, sizeof(myUser.ready), 0);
        } else if (gameData.gameMode == 1 && myUser.id == gameData.drawingUser) {
            switch (ch) {
                case KEY_UP:    cursor.x = (cursor.x > 0) ? cursor.x - 1 : cursor.x; break;
                case KEY_DOWN:  cursor.x = (cursor.x < MAX_WIDTH - 1) ? cursor.x + 1 : cursor.x; break;
                case KEY_LEFT:  cursor.y = (cursor.y > 0) ? cursor.y - 1 : cursor.y; break;
                case KEY_RIGHT: cursor.y = (cursor.y < MAX_HEIGHT - 1) ? cursor.y + 1 : cursor.y; break;
                case 't':       cursor.tool_type = !cursor.tool_type; break;
            }

            char ch_draw = (cursor.tool_type) ? 'O' : ' ';
            gameData.drawing[cursor.x][cursor.y] = ch_draw;

            PixelUpdate p = { cursor.x, cursor.y, ch_draw };
            send(sockfd, &p, sizeof(PixelUpdate), 0);
        }
    }

    endwin();
}

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("Usage: %s <username> <server_ip>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in serverAddr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[2], &serverAddr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    strcpy(myUser.username, argv[1]);
    myUser.ready = 0;
    myUser.points = 0;

    send(sockfd, myUser.username, sizeof(myUser.username), 0);
    recv(sockfd, &myUser.id, sizeof(myUser.id), 0);

    gameLoop();
    close(sockfd);
    return 0;
}
