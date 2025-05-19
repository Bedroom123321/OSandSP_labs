#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <locale.h>

#define MAX_BUFFER 1024
#define MAX_PATH 256
#define INFO_MESSAGE "Вас приветствует учебный сервер 'MyServer'!\n"

char root_dir[MAX_PATH];
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_sock = -1;
volatile sig_atomic_t running = 1;

void signal_handler(int sig) {
    running = 0;
    if (server_sock >= 0) {
        close(server_sock);
    }
}

void get_timestamp(char *buffer, size_t size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm = localtime(&ts.tv_sec);
    snprintf(buffer, size, "%04d.%02d.%02d-%02d:%02d:%02d.%03ld",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, ts.tv_nsec / 1000000);
}

void log_message(const char *message) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    pthread_mutex_lock(&log_mutex);
    printf("%s %s\n", timestamp, message);
    fflush(stdout);
    pthread_mutex_unlock(&log_mutex);
}

int is_within_root(const char *path) {
    char resolved_path[MAX_PATH];
    char resolved_root[MAX_PATH];
    if (!realpath(path, resolved_path) || !realpath(root_dir, resolved_root)) {
        return 0;
    }
    return strncmp(resolved_root, resolved_path, strlen(resolved_root)) == 0;
}

void handle_list(int client_sock, const char *current_dir) {
    DIR *dir = opendir(current_dir);
    if (!dir) {
        dprintf(client_sock, "Error: Cannot open directory\n");
        return;
    }

    struct dirent *entry;
    char buffer[MAX_BUFFER];
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        struct stat st;
        char full_path[MAX_PATH];
        snprintf(full_path, MAX_PATH, "%s/%s", current_dir, entry->d_name);
        if (lstat(full_path, &st) < 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            snprintf(buffer, MAX_BUFFER, "%s/\n", entry->d_name);
        } else if (S_ISREG(st.st_mode)) {
            snprintf(buffer, MAX_BUFFER, "%s\n", entry->d_name);
        } else if (S_ISLNK(st.st_mode)) {
            char link_target[MAX_PATH];
            ssize_t len = readlink(full_path, link_target, MAX_PATH - 1);
            if (len >= 0) {
                link_target[len] = '\0';
                struct stat target_st;
                if (lstat(link_target, &target_st) == 0) {
                    if (S_ISLNK(target_st.st_mode)) {
                        snprintf(buffer, MAX_BUFFER, "%s -->> %s\n", entry->d_name, link_target);
                    } else {
                        snprintf(buffer, MAX_BUFFER, "%s --> %s\n", entry->d_name, link_target);
                    }
                } else {
                    snprintf(buffer, MAX_BUFFER, "%s --> %s\n", entry->d_name, link_target);
                }
            } else {
                snprintf(buffer, MAX_BUFFER, "%s --> [broken]\n", entry->d_name);
            }
        }
        dprintf(client_sock, "%s", buffer);
    }
    dprintf(client_sock, "\n");
    closedir(dir);
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    char buffer[MAX_BUFFER];
    char current_dir[MAX_PATH];
    strncpy(current_dir, root_dir, MAX_PATH);

    dprintf(client_sock, "%s", INFO_MESSAGE);

    while (1) {
        int bytes_read = read(client_sock, buffer, MAX_BUFFER - 1);
        if (bytes_read <= 0) break;

        buffer[bytes_read] = '\0';
        char *command = strtok(buffer, " \n");
        if (!command) continue;

        char log_msg[MAX_BUFFER];
        snprintf(log_msg, MAX_BUFFER, "Received command: %s", command);
        log_message(log_msg);

        if (strcmp(command, "ECHO") == 0) {
            char *text = strtok(NULL, "\n");
            if (text) {
                dprintf(client_sock, "%s\n", text);
            } else {
                dprintf(client_sock, "\n");
            }
        } else if (strcmp(command, "QUIT") == 0) {
            dprintf(client_sock, "BYE\n");
            break;
        } else if (strcmp(command, "INFO") == 0) {
            dprintf(client_sock, "%s", INFO_MESSAGE);
        } else if (strcmp(command, "CD") == 0) {
            char *dir = strtok(NULL, " \n");
            if (dir) {
                char new_dir[MAX_PATH];
                if (dir[0] == '/') {
                    snprintf(new_dir, MAX_PATH, "%s%s", root_dir, dir);
                } else {
                    snprintf(new_dir, MAX_PATH, "%s/%s", current_dir, dir);
                }

                char resolved_path[MAX_PATH];
                if (realpath(new_dir, resolved_path) && is_within_root(resolved_path)) {
                    strncpy(current_dir, resolved_path, MAX_PATH);
                }
                dprintf(client_sock, "\n"); // Пустой ответ для синхронизации
            }
        } else if (strcmp(command, "LIST") == 0) {
            handle_list(client_sock, current_dir);
        } else {
            dprintf(client_sock, "Unknown command\n");
        }
    }

    log_message("Client disconnected");
    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s root_dir port_no\n", argv[0]);
        return 1;
    }

    setlocale(LC_ALL, "ru_RU.UTF-8");

    signal(SIGINT, signal_handler);

    strncpy(root_dir, argv[1], MAX_PATH);
    int port = atoi(argv[2]);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_sock);
        return 1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen");
        close(server_sock);
        return 1;
    }

    printf("Готов.\n");
    log_message("Server started");

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (*client_sock < 0) {
            if (!running) {
                free(client_sock);
                break;
            }
            perror("accept");
            free(client_sock);
            continue;
        }

        log_message("New client connected");

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, client_sock) != 0) {
            perror("pthread_create");
            close(*client_sock);
            free(client_sock);
        }
        pthread_detach(thread);
    }

    log_message("Server shutting down");
    close(server_sock);
    return 0;
}