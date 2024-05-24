#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>

#define DB_SIZE 10
#define BUFFER_SIZE 256
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MONITOR_PORT 8081

typedef struct {
    int data[DB_SIZE];
    pthread_mutex_t mutex;
} SharedMemory;

int monitor_fd;  // Файловый дескриптор для монитора

// Функция вычисления факториала
unsigned long long factorial(int n) {
    if (n > 20) return 0;  // Предотвращение переполнения
    unsigned long long result = 1;
    for (int i = 1; i <= n; ++i) {
        result *= i;
    }
    return result;
}

// Обработчик запросов клиента
void handle_client(int sock, SharedMemory *shm) {
    char buffer[BUFFER_SIZE];
    int read_size;

    printf("Handling client with PID: %d\n", getpid());

    while ((read_size = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[read_size] = '\0';
        int idx = rand() % DB_SIZE;

        pthread_mutex_lock(&shm->mutex);
        int value = shm->data[idx];
        pthread_mutex_unlock(&shm->mutex);

        if (strncmp(buffer, "read", 4) == 0) {
            unsigned long long fact = factorial(value);
            char response[BUFFER_SIZE];
            memset(response, 0, sizeof(response));  // Обнуляем буфер перед использованием
            int length = snprintf(response, sizeof(response), "Reader %d: index %d, value %d, factorial %llu", getpid(), idx, value, fact);
            if (length < 0 || length >= sizeof(response)) {
                fprintf(stderr, "Error: response buffer overflow or encoding error\n");
            } else {
                printf("Debug (formatted read): %s\n", response);  // Отладка перед отправкой
                if (send(sock, response, length, 0) < 0) {
                    perror("send failed");
                }
                // Отправка данных на монитор
                if (write(monitor_fd, response, length) < 0) {
                    perror("write to monitor failed");
                }
            }
        } else if (strncmp(buffer, "write", 5) == 0) {
            int old_value = value;
            int new_value = rand() % 20 + 1;  // Новое значение не больше 20
            shm->data[idx] = new_value;

            char response[BUFFER_SIZE];
            memset(response, 0, sizeof(response));  // Обнуляем буфер перед использованием
            int length = snprintf(response, sizeof(response), "Writer %d: index %d, old value %d, new value %d", getpid(), idx, old_value, new_value);
            if (length < 0 || length >= sizeof(response)) {
                fprintf(stderr, "Error: response buffer overflow or encoding error\n");
            } else {
                printf("Debug (formatted write): %s\n", response);  // Отладка перед отправкой
                if (send(sock, response, length, 0) < 0) {
                    perror("send failed");
                }
                // Отправка данных на монитор
                if (write(monitor_fd, response, length) < 0) {
                    perror("write to monitor failed");
                }
            }
        }
    }

    if (read_size == 0) {
        printf("Client disconnected\n");
    } else if (read_size == -1) {
        perror("recv failed");
    }

    close(sock);
    printf("Connection closed for client with PID: %d\n", getpid());  // Отладка при закрытии соединения
    exit(0);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server, client;
    socklen_t client_len = sizeof(struct sockaddr_in);

    // Setup shared memory
    int shm_fd = shm_open("/shm_database", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(SharedMemory));
    SharedMemory *shm = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    // Initialize mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm->mutex, &attr);

    // Initialize shared data
    for (int i = 0; i < DB_SIZE; i++) {
        shm->data[i] = rand() % 20 + 1;
    }

    // Print initial values for debugging
    printf("Initial values in shared memory:\n");
    for (int i = 0; i < DB_SIZE; i++) {
        printf("Index %d: %d\n", i, shm->data[i]);
    }

    // Setup socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    listen(server_fd, 3);
    printf("Server is waiting for incoming connections on %s:%d...\n", SERVER_IP, SERVER_PORT);

    // Setup monitor socket
    monitor_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (monitor_fd == -1) {
        perror("Could not create monitor socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in monitor_addr;
    monitor_addr.sin_family = AF_INET;
    monitor_addr.sin_addr.s_addr = INADDR_ANY;
    monitor_addr.sin_port = htons(MONITOR_PORT);

    if (bind(monitor_fd, (struct sockaddr *)&monitor_addr, sizeof(monitor_addr)) < 0) {
        perror("Monitor bind failed");
        exit(EXIT_FAILURE);
    }

    listen(monitor_fd, 1);
    printf("Monitor is waiting for incoming connections on %s:%d...\n", SERVER_IP, MONITOR_PORT);

    int monitor_client_fd = accept(monitor_fd, NULL, NULL);
    if (monitor_client_fd < 0) {
        perror("Monitor accept failed");
        exit(EXIT_FAILURE);
    }

    monitor_fd = monitor_client_fd;
    printf("Monitor connected\n");

    while ((client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len))) {
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd, shm);
        } else if (pid > 0) {
            close(client_fd);
        } else {
            perror("fork failed");
        }
    }

    close(server_fd);
    shm_unlink("/shm_database");
    while (wait(NULL) > 0);  // Wait for all child processes
    return 0;
}
