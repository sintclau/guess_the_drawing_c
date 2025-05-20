#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>

#define PORT 12345
#define MAX_CLIENTS 10
#define DRAWING_WIDTH 40
#define DRAWING_HEIGHT 20
#define BUFFER_SIZE (DRAWING_WIDTH * DRAWING_HEIGHT + DRAWING_HEIGHT + 100)
#define ROUND_DURATION 90

typedef struct {
    int socket;
    char username[32];
    int score;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;
char drawing[DRAWING_HEIGHT][DRAWING_WIDTH];
char word_to_guess[64];
int drawing_player_index = -1;
time_t round_start_time;

const char **words;
int word_count;
void load_words(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open words file");
        exit(EXIT_FAILURE);
    }

    static char *word_list[1024];
    static char buffer[64];
    int count = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\n")] = '\0';
        word_list[count] = strdup(buffer);
        count++;
    }

    fclose(file);
    words = (const char **)word_list;
    word_count = count;
}

void broadcast(const char *message, int exclude_socket) {
    for (int i = 0; i < client_count; ++i) {
        if (clients[i].socket != exclude_socket) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
}

void send_roles_to_all() {
    for (int i = 0; i < client_count; ++i) {
        if (i == drawing_player_index) {
            send(clients[i].socket, "ROLE:DRAW\n", 10, 0);
        } else {
            send(clients[i].socket, "ROLE:GUESS\n", 11, 0);
        }
    }
}

void send_drawer_name_to_all() {
    char msg[64];
    snprintf(msg, sizeof(msg), "DRAWER:%s\n", clients[drawing_player_index].username);
    broadcast(msg, -1);
}

void send_scores_to_all() {
    char buffer[1024] = "";
    for (int i = 0; i < client_count; ++i) {
        char line[128];
        snprintf(line, sizeof(line), "PLAYER:%s:%d\n", clients[i].username, clients[i].score);
        strcat(buffer, line);
    }
    broadcast(buffer, -1);
}

void send_hint_to_guessers() {
    char hint[128] = "HINT:";
    for (size_t i = 0; i < strlen(word_to_guess); ++i) {
        strcat(hint, "_ ");
    }
    strcat(hint, "\n");

    broadcast(hint, clients[drawing_player_index].socket);
}

void send_hint_to_client(int socket) {
    char hint[128] = "HINT:";
    for (size_t i = 0; i < strlen(word_to_guess); ++i) {
        strcat(hint, "_ ");
    }
    strcat(hint, "\n");
    send(socket, hint, strlen(hint), 0);
}

void send_word_to_drawer() {
    char word_msg[128];
    snprintf(word_msg, sizeof(word_msg), "WORD:%s\n", word_to_guess);
    send(clients[drawing_player_index].socket, word_msg, strlen(word_msg), 0);
}

int word_index;

void init_word() {
    srand(time(NULL));
    word_index = rand() % word_count;
}

void choose_new_word() {
    strcpy(word_to_guess, words[word_index]);
    printf("New word to guess: %s\n", word_to_guess);
    round_start_time = time(NULL);

    word_index = (word_index + 1) % word_count;
}

void send_drawing_to_all() {
    char buffer[BUFFER_SIZE];
    strcpy(buffer, "DRAWING_UPDATE\n");
    for (int i = 0; i < DRAWING_HEIGHT; ++i) {
        strncat(buffer, drawing[i], DRAWING_WIDTH);
        strcat(buffer, "\n");
    }
    broadcast(buffer, -1);
}

void reset_drawing() {
    for (int i = 0; i < DRAWING_HEIGHT; ++i) {
        for (int j = 0; j < DRAWING_WIDTH; ++j) {
            drawing[i][j] = ' ';
        }
    }
}

void remove_client(int index) {
    close(clients[index].socket);
    for (int i = index; i < client_count - 1; ++i) {
        clients[i] = clients[i + 1];
    }
    client_count--;
    if (drawing_player_index >= client_count) {
        drawing_player_index = 0;
    }
}

void start_new_round() {
    if (client_count == 0) return;
    choose_new_word();
    send_word_to_drawer();
    send_hint_to_guessers();
    send_drawer_name_to_all();
}

void handle_guess(int client_index, const char *guess) {
    if (strcasecmp(guess, word_to_guess) == 0 && client_count > 0) {
        clients[client_index].score += 10;
        char msg[128];
        snprintf(msg, sizeof(msg), "CORRECT! %s guessed the word!\n", clients[client_index].username);
        broadcast(msg, -1);
        send(clients[client_index].socket, msg, strlen(msg), 0);

        drawing_player_index = (drawing_player_index + 1) % client_count;
        send_roles_to_all();

        reset_drawing();
        send_drawing_to_all();

        start_new_round();
    }
}

int skip_votes[MAX_CLIENTS] = {0};

void check_skip_votes() {
    int guessers = 0;
    int votes = 0;

    for (int i = 0; i < client_count; ++i) {
        if (i != drawing_player_index) {
            guessers++;
            if (skip_votes[i]) votes++;
        }
    }

    if (guessers > 0 && votes > guessers / 2) {
        broadcast("Round skipped by majority vote!\n", -1);
        drawing_player_index = (drawing_player_index + 1) % client_count;
        send_roles_to_all();
        reset_drawing();
        send_drawing_to_all();
        start_new_round();
        memset(skip_votes, 0, sizeof(skip_votes));
    }
}

int main() {
    load_words("words.txt");
    init_word();

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    fd_set read_fds;
    printf("Server started on port %d\n", PORT);

    reset_drawing();
    start_new_round();

    time_t last_timer_sent = 0;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        int max_fd = server_fd;

        for (int i = 0; i < client_count; ++i) {
            FD_SET(clients[i].socket, &read_fds);
            if (clients[i].socket > max_fd)
                max_fd = clients[i].socket;
        }

        struct timeval timeout = {1, 0};
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0) {
            perror("select error");
            continue;
        }

        time_t now = time(NULL);
        if (now != last_timer_sent) {
            last_timer_sent = now;
            int elapsed = now - round_start_time;
            int remaining = ROUND_DURATION - elapsed;
            char timer_msg[64];
            snprintf(timer_msg, sizeof(timer_msg), "TIMER:%d\n", remaining);
            broadcast(timer_msg, -1);

            if (remaining <= 0) {
                if (client_count > 0) {
                    char timeout_msg[] = "Nobody guessed in time!\n";
                    broadcast(timeout_msg, -1);
                    drawing_player_index = (drawing_player_index + 1) % client_count;
                    send_roles_to_all();
                    reset_drawing();
                    send_drawing_to_all();
                    start_new_round();
                }
            }
        }

        if (FD_ISSET(server_fd, &read_fds)) {
            int new_socket = accept(server_fd, NULL, NULL);
            if (new_socket >= 0 && client_count < MAX_CLIENTS) {
                recv(new_socket, clients[client_count].username, sizeof(clients[client_count].username), 0);
                clients[client_count].socket = new_socket;
                clients[client_count].score = 0;

                if (client_count == 0) {
                    drawing_player_index = 0;
                }

                client_count++;
                if (client_count == 1) {
                    drawing_player_index = 0;
                    start_new_round();
                }
                if (client_count - 1 != drawing_player_index) {
                    send_hint_to_client(clients[client_count - 1].socket);
                    send_drawer_name_to_all();
                }
                printf("New client connected. Total: %d\n", client_count);
                send_roles_to_all();
            }
        }

        for (int i = 0; i < client_count; ++i) {
            if (FD_ISSET(clients[i].socket, &read_fds)) {
                char buffer[BUFFER_SIZE];
                int valread = recv(clients[i].socket, buffer, sizeof(buffer) - 1, 0);
                if (valread <= 0) {
                    printf("Client %s disconnected\n", clients[i].username);
                    remove_client(i);
                    send_roles_to_all();
                    break;
                }
                buffer[valread] = '\0';

                if (strncmp(buffer, "GUESS:", 6) == 0) {
                    handle_guess(i, buffer + 6);
                } else if (strncmp(buffer, "DRAW:", 5) == 0 && i == drawing_player_index) {
                    char *drawing_data = buffer + 5;
                    int k = 0;
                    for (int y = 0; y < DRAWING_HEIGHT; ++y) {
                        for (int x = 0; x < DRAWING_WIDTH; ++x) {
                            drawing[y][x] = drawing_data[k++];
                        }
                    }
                    send_drawing_to_all();
                } else if (strncmp(buffer, "CLEAR", 5) == 0 && i == drawing_player_index) {
                    reset_drawing();
                    send_drawing_to_all();
                } else if (strncmp(buffer, "SKIP", 4) == 0 && i != drawing_player_index) {
                    skip_votes[i] = 1;
                    check_skip_votes();
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
