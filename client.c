#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include "raylib.h"

#define SERVER_PORT 12345
#define DRAWING_WIDTH 40
#define DRAWING_HEIGHT 20
#define CELL_SIZE 20
#define MAX_INPUT 128
#define ROUND_DURATION 90

char drawing[DRAWING_HEIGHT][DRAWING_WIDTH];
char username[32];
int is_drawing_player = 0;
int socket_fd;

char status_message[128] = "";
float status_timer = 0.0f;
char hint_display[128] = "";
char word_display[64] = "";
char drawer_name[64] = "";

float time_left = ROUND_DURATION;
int connection_lost = 0;
float disconnect_timer = 0.0f;

void *network_thread(void *arg) {
    char buffer[2048];
    while (1) {
        int valread = recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (valread <= 0) {
            connection_lost = 1;
            disconnect_timer = 3.0f;
            break;
        }
        buffer[valread] = '\0';

        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            if (strcmp(line, "ROLE:DRAW") == 0) {
                is_drawing_player = 1;
                printf("You are the drawing player.\n");
                hint_display[0] = '\0';
            } else if (strcmp(line, "ROLE:GUESS") == 0) {
                is_drawing_player = 0;
                printf("You are a guesser.\n");
                word_display[0] = '\0';
            } else if (strncmp(line, "WORD:", 5) == 0) {
                strncpy(word_display, line + 5, sizeof(word_display));
            } else if (strncmp(line, "DRAWER:", 7) == 0) {
                strncpy(drawer_name, line + 7, sizeof(drawer_name));
            } else if (strcmp(line, "DRAWING_UPDATE") == 0) {
                memset(drawing, ' ', sizeof(drawing));
                for (int i = 0; i < DRAWING_HEIGHT; ++i) {
                    line = strtok(NULL, "\n");
                    if (line != NULL) {
                        strncpy(drawing[i], line, DRAWING_WIDTH);
                    }
                }
            } else if (strncmp(line, "CORRECT!", 8) == 0) {
                strncpy(status_message, line, sizeof(status_message));
                status_timer = 3.0f;
                printf("%s\n", line);
            } else if (strncmp(line, "HINT:", 5) == 0) {
                strncpy(hint_display, line + 5, sizeof(hint_display));
            } else if (strncmp(line, "TIMER:", 6) == 0) {
                time_left = atof(line + 6);
            } else {
                printf("%s\n", line);
            }

            line = strtok(NULL, "\n");
        }
    }
    return NULL;
}

void send_drawing_to_server() {
    char buffer[DRAWING_WIDTH * DRAWING_HEIGHT + 10];
    strcpy(buffer, "DRAW:");
    int k = 5;
    for (int y = 0; y < DRAWING_HEIGHT; ++y) {
        for (int x = 0; x < DRAWING_WIDTH; ++x) {
            buffer[k++] = drawing[y][x];
        }
    }
    buffer[k] = '\0';
    send(socket_fd, buffer, strlen(buffer), 0);
}

void draw_grid() {
    for (int y = 0; y < DRAWING_HEIGHT; ++y) {
        for (int x = 0; x < DRAWING_WIDTH; ++x) {
            if (drawing[y][x] != ' ') {
                DrawRectangle(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, DARKGRAY);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <username> <server_ip>\n", argv[0]);
        return 1;
    }

    strcpy(username, argv[1]);
    const char *server_ip = argv[2];

    struct sockaddr_in server_addr;
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    send(socket_fd, username, strlen(username), 0);

    memset(drawing, ' ', sizeof(drawing));

    pthread_t net_thread;
    pthread_create(&net_thread, NULL, network_thread, NULL);

    InitWindow(DRAWING_WIDTH * CELL_SIZE, DRAWING_HEIGHT * CELL_SIZE + 100, "Guess the Drawing");
    SetTargetFPS(30);

    char guess_input[MAX_INPUT] = "";

    while (!WindowShouldClose()) {
        if (connection_lost) {
            disconnect_timer -= GetFrameTime();
            if (disconnect_timer <= 0.0f) {
                break;
            }
        }

        if (status_timer > 0.0f) {
            status_timer -= GetFrameTime();
            if (status_timer < 0.0f) status_timer = 0.0f;
        }

        if (is_drawing_player && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();
            int x = mouse.x / CELL_SIZE;
            int y = mouse.y / CELL_SIZE;
            if (x >= 0 && x < DRAWING_WIDTH && y >= 0 && y < DRAWING_HEIGHT) {
                drawing[y][x] = '#';
                send_drawing_to_server();
            }
        }

        if (is_drawing_player && IsKeyPressed(KEY_C)) {
            send(socket_fd, "CLEAR", 5, 0);
        }

        if (!is_drawing_player) {
            int key = GetCharPressed();
            if (key >= 32 && key <= 126 && strlen(guess_input) < MAX_INPUT - 1) {
                int len = strlen(guess_input);
                guess_input[len] = (char)key;
                guess_input[len + 1] = '\0';
            } else if (key == KEY_BACKSPACE && strlen(guess_input) > 0) {
                guess_input[strlen(guess_input) - 1] = '\0';
            }

            if (IsKeyPressed(KEY_ENTER)) {
                char msg[MAX_INPUT + 10];
                snprintf(msg, sizeof(msg), "GUESS:%s", guess_input);
                send(socket_fd, msg, strlen(msg), 0);
                guess_input[0] = '\0';
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        draw_grid();

        DrawRectangle(10, 10, DRAWING_WIDTH * CELL_SIZE - 20, 20, LIGHTGRAY);
        float timeRatio = time_left / ROUND_DURATION;
        DrawRectangle(10, 10, (int)((DRAWING_WIDTH * CELL_SIZE - 20) * timeRatio), 20, RED);
        DrawText(TextFormat("%.0f sec", time_left), DRAWING_WIDTH * CELL_SIZE - 60, 12, 18, BLACK);

        if (connection_lost) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(RED, 0.6f));
            DrawText("Connection lost", GetScreenWidth() / 2 - 100, GetScreenHeight() / 2 - 20, 30, WHITE);
        } else if (status_timer > 0.0f) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(RAYWHITE, 0.8f));
            DrawText(status_message, 20, GetScreenHeight() / 2 - 20, 30, DARKGREEN);
        } else {
            if (!is_drawing_player) {
                DrawText(TextFormat("Drawer: %s", drawer_name), 10, DRAWING_HEIGHT * CELL_SIZE + 10, 20, DARKGRAY);
                DrawText("Your Guess:", 10, DRAWING_HEIGHT * CELL_SIZE + 35, 20, DARKGRAY);
                DrawText(guess_input, 150, DRAWING_HEIGHT * CELL_SIZE + 35, 20, BLACK);

                Rectangle skipBtn = {DRAWING_WIDTH * CELL_SIZE - 110, DRAWING_HEIGHT * CELL_SIZE + 10, 100, 30};
                DrawRectangleRec(skipBtn, LIGHTGRAY);
                DrawText("Skip", skipBtn.x + 30, skipBtn.y + 5, 20, BLACK);
                if (CheckCollisionPointRec(GetMousePosition(), skipBtn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    send(socket_fd, "SKIP", 4, 0);
                }
            } else {
                DrawText("You are drawing! Press 'C' to clear.", 10, DRAWING_HEIGHT * CELL_SIZE + 10, 20, DARKGRAY);
                DrawText(TextFormat("Word: %s", word_display), 10, DRAWING_HEIGHT * CELL_SIZE + 35, 20, DARKGRAY);
            }

            if (hint_display[0] != '\0') {
                DrawText(TextFormat("Hint: %s", hint_display), 10, DRAWING_HEIGHT * CELL_SIZE + 60, 20, GRAY);
            }
        }

        EndDrawing();
    }

    CloseWindow();
    close(socket_fd);
    return 0;
}
