// admin_client.c - versiune cu shutdown - Samuel Pop

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

void usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s --shutdown\n", prog);
    printf("  (alte opțiuni vor fi adăugate ulterior)\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    int shutdown_flag = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--shutdown") == 0) {
            shutdown_flag = 1;
        }
    }

    if (!shutdown_flag) usage(argv[0]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("[ADMIN] Connected to server.\n");

    if (shutdown_flag) {
        const char *msg = "SHUTDOWN\n";
        send(sock, msg, strlen(msg), 0);  // modificare Samuel Pop
        printf("[ADMIN] Sent SHUTDOWN command to server.\n");
        close(sock);
        return 0;
    }

    close(sock);
    return 0;
}

