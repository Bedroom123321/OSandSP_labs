#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <locale.h>

#define MAX_BUFFER 1024
#define DEFAULT_PORT 8080

void read_server_response(int sock) {
    char buffer[MAX_BUFFER];
    int bytes_read;
    struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        int ready = select(sock + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            perror("select");
            break;
        }
        if (ready == 0) {
            break;
        }

        bytes_read = read(sock, buffer, MAX_BUFFER - 1);
        if (bytes_read <= 0) {
            if (bytes_read < 0 && errno != EAGAIN) {
                perror("read");
            }
            break;
        }

        buffer[bytes_read] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, "\n") && (strlen(buffer) == 1 || strstr(buffer, "BYE"))) {
            break;
        }
    }
}

void process_file(int sock, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Cannot open file %s\n", filename);
        return;
    }

    char line[MAX_BUFFER];
    while (fgets(line, MAX_BUFFER, file)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        printf("> %s\n", line);
        write(sock, line, strlen(line));
        write(sock, "\n", 1);
        read_server_response(sock);
    }
    fclose(file);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s server_domain [port]\n", argv[0]);
        return 1;
    }

    setlocale(LC_ALL, "ru_RU.UTF-8");

    int port = (argc == 3) ? atoi(argv[2]) : DEFAULT_PORT;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address\n");
        close(sock);
        return 1;
    }

    int conn_result = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (conn_result < 0 && errno != EINPROGRESS) {
        perror("connect");
        close(sock);
        return 1;
    }

    if (conn_result < 0 && errno == EINPROGRESS) {
        fd_set writefds;
        struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };
        FD_ZERO(&writefds);
        FD_SET(sock, &writefds);

        if (select(sock + 1, NULL, &writefds, NULL, &tv) > 0) {
            int so_error;
            socklen_t len = sizeof(so_error);
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
            if (so_error != 0) {
                fprintf(stderr, "connect failed: %s\n", strerror(so_error));
                close(sock);
                return 1;
            }
        } else {
            fprintf(stderr, "connect timeout\n");
            close(sock);
            return 1;
        }
    }

    fcntl(sock, F_SETFL, flags);

    read_server_response(sock);

    char buffer[MAX_BUFFER];
    while (1) {
        printf("> ");
        fflush(stdout);
        if (!fgets(buffer, MAX_BUFFER, stdin)) break;
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0) continue;

        if (buffer[0] == '@') {
            process_file(sock, buffer + 1);
            continue;
        }

        write(sock, buffer, strlen(buffer));
        write(sock, "\n", 1);

        read_server_response(sock);

        if (strncmp(buffer, "QUIT", 4) == 0) break;
    }

    close(sock);
    return 0;
}