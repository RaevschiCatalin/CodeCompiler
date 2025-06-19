#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <poll.h>

#define SOCKET_PATH "/tmp/admin_socket"
#define BUFFER_SIZE 1024
#define LOG_FILE "/tmp/admin_client.log"
#define TIMEOUT_SEC 30
#define USE_POLL  // Uncomment this line to enable poll() instead of select()

void log_message(const char *message) {
    FILE *log = fopen(LOG_FILE, "a");
    if (log) {
        time_t now = time(NULL);
        char *timestamp = ctime(&now);
        timestamp[strlen(timestamp) - 1] = '\0';
        fprintf(log, "[%s] %s\n", timestamp, message);
        fclose(log);
    }
}

void clear_screen() {
    printf("\033[2J\033[H");
    fflush(stdout);
}

void print_menu() {
    printf("--- Meniu Admin ---\n");
    printf("1. STATUS server (afiseaza ora exacta)\n");
    printf("2. SHUTDOWN server (inchide serverul si logheaza)\n");
    printf("3. LOGOUT (deconectare client admin)\n");
    printf("4. BLOCK_IP (blocheaza IP si salveaza pe server)\n");
    printf("5. KILL_SERVER (opreste fortat serverul)\n");
    printf("6. GET_LOGS (vezi ultimele linii din log)\n");
    printf("7. UPLOAD_FILE (incarca orice fisier)\n");
    printf("8. DOWNLOAD_REPORT (descarca un raport Excel)\n");
    printf("9. LIST_USERS (afiseaza clientii conectati)\n");
    printf("-------------------\n");
}

int send_file(int sockfd, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Eroare deschidere fisier");
        return -1;
    }
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char size_buffer[64];
    snprintf(size_buffer, sizeof(size_buffer), "%ld", filesize);
    write(sockfd, size_buffer, sizeof(size_buffer));
    usleep(100000);

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        write(sockfd, buffer, bytes_read);
    }

    fclose(file);
    return 0;
}

int receive_file(int sockfd, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Eroare creare fisier download");
        return -1;
    }

    char size_buffer[64];
    if (read(sockfd, size_buffer, sizeof(size_buffer)) <= 0) {
        fclose(file);
        return -1;
    }
    long filesize = atol(size_buffer);

    long received = 0;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while (received < filesize) {
        bytes_read = read(sockfd, buffer, sizeof(buffer));
        if (bytes_read <= 0) break;
        fwrite(buffer, 1, bytes_read, file);
        received += bytes_read;
    }

    fclose(file);
    return 0;
}

int main() {
    int sockfd;
    struct sockaddr_un addr;
    fd_set read_fds;
    struct timeval timeout;
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    int running = 1;

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Conectat la serverul admin.\n");
    log_message("Conectat la serverul admin.");

    // --- LOGIN BASIC ---
    char password[128];
    printf("Introdu parola admin: ");
    fflush(stdout);
    if (!fgets(password, sizeof(password), stdin)) {
        printf("Eroare citire parola.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    password[strcspn(password, "\n")] = 0;
    write(sockfd, password, strlen(password));
    // Asteapta raspuns de la server
    int n = read(sockfd, buffer, sizeof(buffer) - 1);
    if (n <= 0) {
        printf("Serverul nu a raspuns la login.\n");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    buffer[n] = '\0';
    if (strncmp(buffer, "OK", 2) != 0) {
        printf("Autentificare esuata: %s\n", buffer);
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    // --- END LOGIN ---

    // --- SELECT-BASED MULTIPLEXING ---
    while (running) {
        clear_screen();
        print_menu();
        printf("Selecteaza optiunea: ");
        fflush(stdout);

#ifdef USE_POLL
        struct pollfd fds[2];
        fds[0].fd = sockfd;
        fds[0].events = POLLIN;
        fds[1].fd = 0; // stdin
        fds[1].events = POLLIN;

        int activity = poll(fds, 2, TIMEOUT_SEC * 1000);
        if (activity == -1) {
            perror("poll");
            break;
        } else if (activity == 0) {
            printf("Timeout: nicio activitate.\n");
            log_message("Timeout asteptand activitate.");
            break;
        }

        // Prioritate: socket (server) => afiseaza mesaj imediat
        if (fds[0].revents & POLLIN) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("\n[Raspuns server]: %s\n", buffer);
                log_message(buffer);
                if (strcmp(buffer, "LOGOUT") == 0 || strcmp(buffer, "SHUTDOWN") == 0 || strcmp(buffer, "KILL_SERVER") == 0) {
                    printf("Deconectare...\n");
                    break;
                }
                printf("Apasa Enter pentru a reveni la meniu...");
                getchar();
                continue;
            } else {
                printf("Serverul s-a deconectat.\n");
                break;
            }
        }

        // Daca userul tasteaza ceva
        if (fds[1].revents & POLLIN) {
            if (!fgets(command, sizeof(command), stdin)) {
                printf("Eroare citire input.\n");
                continue;
            }
            command[strcspn(command, "\n")] = 0;
            memset(buffer, 0, sizeof(buffer));

            if (strcmp(command, "1") == 0) {
                strcpy(buffer, "STATUS");
            } else if (strcmp(command, "2") == 0) {
                strcpy(buffer, "SHUTDOWN");
            } else if (strcmp(command, "3") == 0) {
                strcpy(buffer, "LOGOUT");
            } else if (strcmp(command, "4") == 0) {
                printf("Introdu IP-ul de blocat: ");
                char ip_buffer[BUFFER_SIZE];
                if (!fgets(ip_buffer, sizeof(ip_buffer), stdin)) {
                    printf("Eroare citire IP.\n");
                    continue;
                }
                ip_buffer[strcspn(ip_buffer, "\n")] = 0;
                snprintf(buffer, sizeof(buffer), "BLOCK_IP %s", ip_buffer);
            } else if (strcmp(command, "5") == 0) {
                strcpy(buffer, "KILL_SERVER");
            } else if (strcmp(command, "6") == 0) {
                strcpy(buffer, "GET_LOGS");
            } else if (strcmp(command, "7") == 0) {
                printf("Introdu calea completa a fisierului de upload: ");
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                write(sockfd, "UPLOAD_FILE", strlen("UPLOAD_FILE"));
                usleep(100000);
                send_file(sockfd, buffer);
                log_message("Fisier trimis serverului.");
                sleep(2);
                continue;
            } else if (strcmp(command, "8") == 0) {
                write(sockfd, "DOWNLOAD_REPORT", strlen("DOWNLOAD_REPORT"));
                usleep(100000);
                receive_file(sockfd, "raport_primire.xlsx");
                printf("Raport descarcat ca 'raport_primire.xlsx'.\n");
                log_message("Raport primit de la server.");
                sleep(2);
                continue;
            } else if (strcmp(command, "9") == 0) {
                strcpy(buffer, "LIST_USERS");
            } else {
                printf("Optiune invalida.\n");
                sleep(2);
                continue;
            }

            if (strcmp(command, "9") == 0) {
                if (write(sockfd, buffer, strlen(buffer)) == -1) {
                    perror("write");
                    break;
                }
                log_message(buffer);
                int n = read(sockfd, buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    printf("\nClienti conectati:\n%s\n", buffer);
                    log_message(buffer);
                } else {
                    printf("Nu s-au putut obtine clientii conectati.\n");
                }
                printf("Apasa Enter pentru a reveni la meniu...");
                getchar();
                continue;
            }

            if (write(sockfd, buffer, strlen(buffer)) == -1) {
                perror("write");
                break;
            }
            log_message(buffer);
            // Raspunsul serverului va fi tratat la urmatorul select
        }
#else
        FD_ZERO(&read_fds);
        FD_SET(0, &read_fds); // stdin
        FD_SET(sockfd, &read_fds); // socket
        int maxfd = (sockfd > 0 ? sockfd : 0) + 1;
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        int activity = select(maxfd, &read_fds, NULL, NULL, &timeout);
        if (activity == -1) {
            perror("select");
            break;
        } else if (activity == 0) {
            printf("Timeout: nicio activitate.\n");
            log_message("Timeout asteptand activitate.");
            break;
        }

        // Prioritate: socket (server) => afiseaza mesaj imediat
        if (FD_ISSET(sockfd, &read_fds)) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("\n[Raspuns server]: %s\n", buffer);
                log_message(buffer);
                if (strcmp(buffer, "LOGOUT") == 0 || strcmp(buffer, "SHUTDOWN") == 0 || strcmp(buffer, "KILL_SERVER") == 0) {
                    printf("Deconectare...\n");
                    break;
                }
                printf("Apasa Enter pentru a reveni la meniu...");
                getchar();
                continue;
            } else {
                printf("Serverul s-a deconectat.\n");
                break;
            }
        }

        // Daca userul tasteaza ceva
        if (FD_ISSET(0, &read_fds)) {
            if (!fgets(command, sizeof(command), stdin)) {
                printf("Eroare citire input.\n");
                continue;
            }
            command[strcspn(command, "\n")] = 0;
            memset(buffer, 0, sizeof(buffer));

            if (strcmp(command, "1") == 0) {
                strcpy(buffer, "STATUS");
            } else if (strcmp(command, "2") == 0) {
                strcpy(buffer, "SHUTDOWN");
            } else if (strcmp(command, "3") == 0) {
                strcpy(buffer, "LOGOUT");
            } else if (strcmp(command, "4") == 0) {
                printf("Introdu IP-ul de blocat: ");
                char ip_buffer[BUFFER_SIZE];
                if (!fgets(ip_buffer, sizeof(ip_buffer), stdin)) {
                    printf("Eroare citire IP.\n");
                    continue;
                }
                ip_buffer[strcspn(ip_buffer, "\n")] = 0;
                snprintf(buffer, sizeof(buffer), "BLOCK_IP %s", ip_buffer);
            } else if (strcmp(command, "5") == 0) {
                strcpy(buffer, "KILL_SERVER");
            } else if (strcmp(command, "6") == 0) {
                strcpy(buffer, "GET_LOGS");
            } else if (strcmp(command, "7") == 0) {
                printf("Introdu calea completa a fisierului de upload: ");
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                write(sockfd, "UPLOAD_FILE", strlen("UPLOAD_FILE"));
                usleep(100000);
                send_file(sockfd, buffer);
                log_message("Fisier trimis serverului.");
                sleep(2);
                continue;
            } else if (strcmp(command, "8") == 0) {
                write(sockfd, "DOWNLOAD_REPORT", strlen("DOWNLOAD_REPORT"));
                usleep(100000);
                receive_file(sockfd, "raport_primire.xlsx");
                printf("Raport descarcat ca 'raport_primire.xlsx'.\n");
                log_message("Raport primit de la server.");
                sleep(2);
                continue;
            } else if (strcmp(command, "9") == 0) {
                strcpy(buffer, "LIST_USERS");
            } else {
                printf("Optiune invalida.\n");
                sleep(2);
                continue;
            }

            if (strcmp(command, "9") == 0) {
                if (write(sockfd, buffer, strlen(buffer)) == -1) {
                    perror("write");
                    break;
                }
                log_message(buffer);
                int n = read(sockfd, buffer, sizeof(buffer) - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    printf("\nClienti conectati:\n%s\n", buffer);
                    log_message(buffer);
                } else {
                    printf("Nu s-au putut obtine clientii conectati.\n");
                }
                printf("Apasa Enter pentru a reveni la meniu...");
                getchar();
                continue;
            }

            if (write(sockfd, buffer, strlen(buffer)) == -1) {
                perror("write");
                break;
            }
            log_message(buffer);
            // Raspunsul serverului va fi tratat la urmatorul select
        }
#endif
    }

    close(sockfd);
    log_message("Client admin inchis.");
    printf("\nClient admin inchis.\n");

    return 0;
}


