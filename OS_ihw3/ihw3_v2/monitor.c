#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define MONITOR_PORT 8081
#define BUFFER_SIZE 256

int main() {
    int sock;
    struct sockaddr_in server;

    // Создание сокета
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Could not create socket");
        return 1;
    }
    printf("Socket created\n");

    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_family = AF_INET;
    server.sin_port = htons(MONITOR_PORT);

    // Подключение к серверу
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed. Error");
        close(sock);
        return 1;
    }

    printf("Connected to server monitor on %s:%d\n", SERVER_IP, MONITOR_PORT);

    // Получение и отображение информации
    char buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("Monitor: %s\n", buffer);
    }

    if (bytes_received < 0) {
        perror("recv failed");
    }

    close(sock);
    return 0;
}
