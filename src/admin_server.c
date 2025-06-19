#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>

#define SOCKET_PATH "/tmp/admin_socket"
#define BUFFER_SIZE 1024
#define LOG_FILE "/tmp/admin_server.log"
#define BLOCKED_IP_FILE "/tmp/blocked_ips.txt"
#define UPLOADS_INFO_FILE "/tmp/uploads_info.txt"
#define UPLOADS_DIR "/tmp/uploads"

int server_sock = -1;
int client_sock = -1;
int admin_connected = 0;

void cleanup() {
    if (client_sock != -1) close(client_sock);
    if (server_sock != -1) close(server_sock);
    unlink(SOCKET_PATH);
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\nServer oprit cu Ctrl+C.\n");
    cleanup();
    exit(0);
}

void log_server_action(const char *action) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strlen(timestamp) - 1] = '\0';
        fprintf(log, "[%s] %s\n", timestamp, action);
        fclose(log);
    }
}

void receive_file(const char *folder_path) {
    mkdir(folder_path, 0777);

    char filename[BUFFER_SIZE];
    snprintf(filename, sizeof(filename), "%s/uploaded_file", folder_path);

    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Eroare creare fisier upload");
        return;
    }

    char size_buffer[64];
    if (read(client_sock, size_buffer, sizeof(size_buffer)) <= 0) {
        fclose(file);
        return;
    }
    long filesize = atol(size_buffer);

    long received = 0;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while (received < filesize) {
        bytes_read = read(client_sock, buffer, sizeof(buffer));
        if (bytes_read <= 0) break;
        fwrite(buffer, 1, bytes_read, file);
        received += bytes_read;
    }

    fclose(file);

    // Logăm numele uploadului
    FILE *uploads_log = fopen(UPLOADS_INFO_FILE, "a");
    if (uploads_log) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strlen(timestamp) - 1] = '\0';
        fprintf(uploads_log, "[%s] Fisier incarcat: uploaded_file (%ld bytes)\n", timestamp, filesize);
        fclose(uploads_log);
    }
}

void send_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Fisierul nu exista pentru download");
        return;
    }

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char size_buffer[64];
    snprintf(size_buffer, sizeof(size_buffer), "%ld", filesize);
    write(client_sock, size_buffer, sizeof(size_buffer));
    usleep(100000);

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        write(client_sock, buffer, bytes_read);
    }

    fclose(file);
}

int count_lines(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    int lines = 0;
    char ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') lines++;
    }

    fclose(file);
    return lines;
}

void send_last_n_lines(const char *filename, int n) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        write(client_sock, "Eroare deschidere log.", strlen("Eroare deschidere log."));
        return;
    }

    int total_lines = count_lines(filename);
    if (n > total_lines) n = total_lines;

    char *lines[total_lines];
    for (int i = 0; i < total_lines; i++) lines[i] = NULL;

    char buffer[BUFFER_SIZE];
    int idx = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        free(lines[idx]);
        lines[idx] = strdup(buffer);
        idx = (idx + 1) % total_lines;
    }

    fclose(file);

    // Trimitem ultimele n linii
    char output[BUFFER_SIZE * 4] = {0};
    int start = (idx + total_lines - n) % total_lines;

    for (int i = 0; i < n; i++) {
        int pos = (start + i) % total_lines;
        if (lines[pos]) {
            strncat(output, lines[pos], sizeof(output) - strlen(output) - 1);
        }
    }

    write(client_sock, output, strlen(output));

    for (int i = 0; i < total_lines; i++) {
        free(lines[i]);
    }
}

void handle_client(int sockfd) {
    if (admin_connected) {
        const char *msg = "ERROR: Another admin client is already connected.\n";
        write(sockfd, msg, strlen(msg));
        close(sockfd);
        return;
    }
    admin_connected = 1;
    char buffer[BUFFER_SIZE];

    // --- LOGIN BASIC ---
    memset(buffer, 0, sizeof(buffer));
    int bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        close(sockfd);
        admin_connected = 0;
        return;
    }
    buffer[bytes_read] = '\0';
    // buffer conține parola
    const char *admin_pass = "admin123";
    if (strcmp(buffer, admin_pass) != 0) {
        const char *msg = "ERROR: Parola gresita!\n";
        write(sockfd, msg, strlen(msg));
        close(sockfd);
        admin_connected = 0;
        return;
    } else {
        const char *msg = "OK";
        write(sockfd, msg, strlen(msg));
    }
    // --- END LOGIN ---

    // Multiplexare cu select
    fd_set fds;
    int maxfd = sockfd;
    while (1) {
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int sel = select(maxfd + 1, &fds, NULL, NULL, &tv);
        if (sel < 0) {
            perror("select");
            break;
        } else if (sel == 0) {
            continue; // timeout, reluam
        }
        if (FD_ISSET(sockfd, &fds)) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);
            if (bytes_read <= 0) {
                printf("Client deconectat.\n");
                close(sockfd);
                break;
            }
            buffer[bytes_read] = '\0';

            printf("Comanda primita: %s\n", buffer);

            if (strcmp(buffer, "KILL_SERVER") == 0) {
                FILE *pidf = fopen("/tmp/main_server.pid", "r");
                if (pidf) {
                    int pid = 0;
                    fscanf(pidf, "%d", &pid);
                    fclose(pidf);
                    if (pid > 0) {
                        kill(pid, SIGTERM);
                        write(sockfd, "Main server killed.\n", 20);
                    } else {
                        write(sockfd, "Main server PID not found.\n", 28);
                    }
                } else {
                    write(sockfd, "Main server PID file not found.\n", 33);
                }
                log_server_action("KILL_SERVER initiat de Admin");
                cleanup();
                exit(0);
            } else if (strcmp(buffer, "LIST_USERS") == 0) {
                FILE *fp = fopen("/tmp/connected_clients.txt", "r");
                if (!fp) {
                    write(sockfd, "No users connected.\n", 21);
                } else {
                    char line[128];
                    while (fgets(line, sizeof(line), fp)) {
                        write(sockfd, line, strlen(line));
                    }
                    fclose(fp);
                }
            } else if (strcmp(buffer, "STATUS") == 0) {
                time_t now = time(NULL);
                char *timestamp = ctime(&now);
                timestamp[strlen(timestamp) - 1] = '\0';
                char response[BUFFER_SIZE];
                snprintf(response, sizeof(response), "Server functional la %s.", timestamp);
                write(sockfd, response, strlen(response));
            } else if (strcmp(buffer, "SHUTDOWN") == 0) {
                write(sockfd, "SHUTDOWN", strlen("SHUTDOWN"));
                log_server_action("Shutdown initiat de Admin");
                cleanup();
                exit(0);
            } else if (strcmp(buffer, "LOGOUT") == 0) {
                write(sockfd, "LOGOUT", strlen("LOGOUT"));
                close(sockfd);
                break;
            } else if (strncmp(buffer, "BLOCK_IP", 8) == 0) {
                FILE *ipfile = fopen(BLOCKED_IP_FILE, "a");
                if (ipfile) {
                    fprintf(ipfile, "%s\n", buffer + 9);
                    fclose(ipfile);
                }
                write(sockfd, "IP blocat.\n", 11);
            } else if (strcmp(buffer, "GET_LOGS") == 0) {
                int total = count_lines(LOG_FILE);
                char msg[128];
                snprintf(msg, sizeof(msg), "Fisierul log are %d linii. Cate vrei?", total);
                write(sockfd, msg, strlen(msg));
                int n = 0;
                read(sockfd, msg, sizeof(msg));
                n = atoi(msg);
                send_last_n_lines(LOG_FILE, n);
            } else if (strcmp(buffer, "UPLOAD_FILE") == 0) {
                receive_file(UPLOADS_DIR);
                write(sockfd, "Fisier uploadat.\n", 17);
            } else if (strcmp(buffer, "DOWNLOAD_REPORT") == 0) {
                send_file("raport_primire.xlsx");
            } else {
                write(sockfd, "Comanda necunoscuta.\n", 22);
            }
        }
    }
    admin_connected = 0;
}

int main() {
    struct sockaddr_un addr;

    signal(SIGINT, handle_sigint);

    if ((server_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(server_sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("bind");
        cleanup();
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) == -1) {
        perror("listen");
        cleanup();
        exit(EXIT_FAILURE);
    }

    printf("Server admin pornit pe %s...\n", SOCKET_PATH);

    mkdir(UPLOADS_DIR, 0777);

    while (1) {
        if ((client_sock = accept(server_sock, NULL, NULL)) == -1) {
            perror("accept");
            continue;
        }

        printf("Client admin conectat.\n");
        handle_client(client_sock);
        client_sock = -1;
    }

    cleanup();
    return 0;
}


