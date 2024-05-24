#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 256

// Функция для отправки запросов на чтение или запись
void send_request(int sock, const char *request) {
    if (send(sock, request, strlen(request), 0) < 0) {
        puts("Send failed");
        exit(1);
    }

    char server_reply[BUFFER_SIZE];
    memset(server_reply, 0, BUFFER_SIZE);  // Обнуляем буфер перед использованием

    int bytes_received = recv(sock, server_reply, sizeof(server_reply) - 1, 0);
    if (bytes_received < 0) {
        puts("recv failed");
        exit(1);
    }

    server_reply[bytes_received] = '\0';  // Null-terminate the received string
    printf("Server reply: %s\n", server_reply);
}

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in server;

    // Проверка аргументов
    if (argc != 2) {
        fprintf(stderr, "Usage: %s [reader|writer]\n", argv[0]);
        return 1;
    }

    // Создание сокета
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        printf("Could not create socket");
        return 1;
    }
    printf("Socket created\n");

    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);

    // Подключение к серверу
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed. Error");
        return 1;
    }

    printf("Connected to server\n");

    // Читатель или писатель
    if (strcmp(argv[1], "reader") == 0) {
        while (1) {
            printf("Sending read request...\n");  // Отладка
            send_request(sock, "read");
            sleep(1);  // Эмуляция работы читателя
        }
    } else if (strcmp(argv[1], "writer") == 0) {
        while (1) {
            printf("Sending write request...\n");  // Отладка
            send_request(sock, "write");
            sleep(1);  // Эмуляция работы писателя
        }
    } else {
        fprintf(stderr, "Invalid argument. Use 'reader' or 'writer'.\n");
        close(sock);
        return 1;
    }

    close(sock);
    return 0;
}


