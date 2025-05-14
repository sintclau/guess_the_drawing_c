// --- server.c --- //
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS 10
#define PORT 9000
#define MAX_WIDTH 40
#define MAX_HEIGHT 120

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
    int x, y;
    char value;
} PixelUpdate;

gameDataS gameData;
user userList[MAX_CLIENTS];
int clientSockets[MAX_CLIENTS];
int numClients = 0;

void sendGameDataToAllClients() {
    for (int i = 0; i < numClients; i++) {
        char type = TYPE_GAME_DATA;
        send(clientSockets[i], &type, 1, 0);
        send(clientSockets[i], &gameData, sizeof(gameDataS), 0);

        type = TYPE_USER_LIST;
        send(clientSockets[i], &type, 1, 0);
        send(clientSockets[i], userList, sizeof(userList), 0);
    }
}

void removeClient(int index) {
    printf("%s disconnected.\n", userList[index].username);
    close(clientSockets[index]);

    for (int i = index; i < numClients - 1; i++) {
        clientSockets[i] = clientSockets[i + 1];
        userList[i] = userList[i + 1];
    }

    clientSockets[numClients - 1] = 0;
    memset(&userList[numClients - 1], 0, sizeof(user));
    numClients--;

    if (numClients < 1) {
        printf("No players left. Resetting game.\n");
        gameData.gameMode = 0;
        memset(gameData.drawing, ' ', sizeof(gameData.drawing));
        for (int j = 0; j < MAX_CLIENTS; j++) {
            userList[j].ready = 0;
        }
    }
}

int main() {
    gameData.gameMode = 0;
    strcpy(gameData.word, "");
    memset(gameData.drawing, 0, sizeof(gameData.drawing));

    int server_fd, new_socket, max_sd, activity, sd;
    struct sockaddr_in address;
    fd_set readfds;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, MAX_CLIENTS);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < numClients; i++) {
            sd = clientSockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        struct timeval timeout = {0, 50000};
        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if (activity > 0) {
            if (FD_ISSET(server_fd, &readfds)) {
                new_socket = accept(server_fd, NULL, NULL);
                if (numClients < MAX_CLIENTS) {
                    clientSockets[numClients++] = new_socket;
                } else {
                    close(new_socket);
                }
            }

            for (int i = 0; i < numClients; i++) {
                sd = clientSockets[i];
                if (FD_ISSET(sd, &readfds)) {
                    int valread;
                    if (strcmp(userList[i].username, "") == 0) {
                        valread = recv(sd, userList[i].username, sizeof(userList[i].username), 0);
                        if (valread == 0) {
                            removeClient(i);
                        } else {
                            printf("%s connected.\n", userList[i].username);
                            userList[i].id = i;
                            send(sd, &i, sizeof(i), 0);
                        }
                    } else {
                        switch (gameData.gameMode) {
                            case 0:
                                valread = recv(sd, &userList[i].ready, sizeof(userList[i].ready), 0);
                                break;
                            case 1:
                                if (userList[i].id == gameData.drawingUser) {
                                    PixelUpdate pixelUpdate;
                                    valread = recv(sd, &pixelUpdate, sizeof(PixelUpdate), 0);

                                    if (valread > 0) {
                                        if (pixelUpdate.x >= 0 && pixelUpdate.x < MAX_WIDTH &&
                                            pixelUpdate.y >= 0 && pixelUpdate.y < MAX_HEIGHT) {

                                            gameData.drawing[pixelUpdate.x][pixelUpdate.y] = pixelUpdate.value;

                                            for (int j = 0; j < numClients; j++) {
                                                char type = TYPE_PIXEL_UPDATE;
                                                send(clientSockets[j], &type, 1, 0);
                                                send(clientSockets[j], &pixelUpdate, sizeof(PixelUpdate), 0);
                                            }
                                        }
                                    }
                                }
                                break;
                        }

                        if (valread == 0) {
                            removeClient(i);
                        } else {
                            if (gameData.gameMode == 0) {
                                int allReady = 1;
                                for (int j = 0; j < numClients; j++) {
                                    if (!userList[j].ready) {
                                        allReady = 0;
                                        break;
                                    }
                                }
                                if (allReady) {
                                    gameData.gameMode = 1;
                                    gameData.drawingUser = rand() % numClients;
                                    printf("Game started! Drawing user: %s\n", userList[gameData.drawingUser].username);
                                }
                            }
                        }
                    }
                }
            }
        }
        sendGameDataToAllClients();
    }
    return 0;
}
