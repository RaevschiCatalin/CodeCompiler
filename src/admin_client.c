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

#define SOCKET_PATH "/tmp/admin_socket"
#define BUFFER_SIZE 1024
#define LOG_FILE "/tmp/admin_client.log"
#define TIMEOUT_SEC 30
#define INACTIVITY_TIMEOUT 90  

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
    printf("2. SHUTDOWN server (inchide serverul si delogheaza adminu[admin only])\n");
    printf("3. LOGOUT (deconectare client admin)\n");
    printf("4. BLOCK_IP (blocheaza IP si salveaza pe server)\n");
    printf("5. KILL_SERVER (opreste fortat serverul)\n");
    printf("6. GET_LOGS (vezi ultimele linii din log)\n");
    printf("7. UPLOAD_FILE (incarca orice fisier)\n");
    printf("8. LIST_USERS (afiseaza clientii conectati)\n");
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
    time_t last_activity = time(NULL);

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

    while (running) {
       
        time_t current_time = time(NULL);
        if (current_time - last_activity > INACTIVITY_TIMEOUT) {
            printf("\nTimeout de inactivitate (%d secunde). Deconectare...\n", INACTIVITY_TIMEOUT);
            log_message("Timeout de inactivitate - deconectare automata");
            break;
        }

        clear_screen();
        print_menu();
        printf("Selecteaza optiunea (timeout inactivitate: %ld secunde ramase): ", 
               INACTIVITY_TIMEOUT - (current_time - last_activity));

        
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        timeout.tv_sec = 1; 
        timeout.tv_usec = 0;

        int activity = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity == -1) {
            perror("select");
            break;
        } else if (activity == 0) {
            
            continue;
        }

        
        last_activity = time(NULL);

        if (!fgets(command, sizeof(command), stdin)) {
            printf("Eroare citire input.\n");
            continue;
        }
        command[strcspn(command, "\n")] = 0;

        
        static time_t last_heartbeat = 0;
        if (current_time - last_heartbeat >= 60) {
            
            if (write(sockfd, "HEARTBEAT", strlen("HEARTBEAT")) > 0) {
                last_heartbeat = current_time;
            }
        }

        memset(buffer, 0, sizeof(buffer));

        if (strcmp(command, "1") == 0) {
            strcpy(buffer, "STATUS");
            log_message("Actiune aleasa: STATUS - Verificare stare server");
            
        } else if (strcmp(command, "2") == 0) {
            strcpy(buffer, "SHUTDOWN");
            log_message("Actiune aleasa: SHUTDOWN - Oprire normala server");
        } else if (strcmp(command, "3") == 0) {
            strcpy(buffer, "LOGOUT");
            log_message("Actiune aleasa: LOGOUT - Deconectare client admin");
        } else if (strcmp(command, "4") == 0) {
            printf("Introdu IP-ul de blocat: ");
            char ip_buffer[BUFFER_SIZE];
            if (!fgets(ip_buffer, sizeof(ip_buffer), stdin)) {
                printf("Eroare citire IP.\n");
                log_message("Actiune aleasa: BLOCK_IP - Eroare: Nu s-a putut citi IP-ul");
                continue;
            }
            ip_buffer[strcspn(ip_buffer, "\n")] = 0;
            char log_msg[BUFFER_SIZE + 50];
            snprintf(log_msg, sizeof(log_msg), "Actiune aleasa: BLOCK_IP - Blocare acces pentru IP: %s", ip_buffer);
            log_message(log_msg);

            snprintf(buffer, sizeof(buffer), "BLOCK_IP %s", ip_buffer);
        } else if (strcmp(command, "5") == 0) {
            strcpy(buffer, "KILL_SERVER");
            log_message("Actiune aleasa: KILL_SERVER - Oprire fortata server");
            if (write(sockfd, buffer, strlen(buffer)) == -1) {
                perror("write");
                log_message("Rezultat: Eroare la trimiterea comenzii KILL_SERVER");
                break;
            }
            log_message("Rezultat: Comanda KILL_SERVER trimisa - inchidere client admin");
            printf("Comanda KILL_SERVER trimisa. Inchidere client admin...\n");
            close(sockfd);
            exit(0);
        } else if (strcmp(command, "6") == 0) {
            strcpy(buffer, "GET_LOGS");
            log_message("Actiune aleasa: GET_LOGS - Solicitare vizualizare jurnal server");
        } else if (strcmp(command, "7") == 0) {
            printf("Introdu calea completa a fisierului de upload: ");
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0;
            char log_msg[BUFFER_SIZE + 50];
            snprintf(log_msg, sizeof(log_msg), "Actiune aleasa: UPLOAD_FILE - Incarcare fisier: %s", buffer);
            log_message(log_msg);
            write(sockfd, "UPLOAD_FILE", strlen("UPLOAD_FILE"));
            usleep(100000);
            if (send_file(sockfd, buffer) == 0) {
                log_message("Rezultat: Fisier incarcat cu succes");
            } else {
                log_message("Rezultat: Eroare la incarcarea fisierului");
            }
            sleep(2);
            continue;
        } else if (strcmp(command, "8") == 0) {
            strcpy(buffer, "LIST_USERS");
            log_message("Actiune aleasa: LIST_USERS - Listare clienti conectati");
        } else {
            printf("Optiune invalida.\n");
            sleep(2);
            continue;
        }

        if (strcmp(command, "8") == 0) {
            if (write(sockfd, buffer, strlen(buffer)) == -1) {
                perror("write");
                log_message("Rezultat: Eroare la trimiterea comenzii LIST_USERS");
                break;
            }
            int n = read(sockfd, buffer, sizeof(buffer) - 1);
            if (n > 0) {
                buffer[n] = '\0';
                printf("\nClienti conectati:\n%s\n", buffer);
                char log_msg[BUFFER_SIZE + 50];
                snprintf(log_msg, sizeof(log_msg), "Rezultat: Lista clienti obtinuta cu succes - %s", buffer);
                log_message(log_msg);
            } else {
                printf("Nu s-au putut obtine clientii conectati.\n");
                log_message("Rezultat: Eroare la obtinerea listei de clienti");
            }
            printf("Apasa Enter pentru a reveni la meniu...");
            getchar();
            continue;
        }

        if (write(sockfd, buffer, strlen(buffer)) == -1) {
            perror("write");
            log_message("Rezultat: Eroare la trimiterea comenzii catre server");
            break;
        }

        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        timeout.tv_sec = TIMEOUT_SEC;
        timeout.tv_usec = 0;

        int server_activity = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);

        if (server_activity == -1) {
            perror("select");
            log_message("Rezultat: Eroare la asteptarea raspunsului de la server");
            break;
        } else if (server_activity == 0) {
            printf("Timeout: serverul nu a raspuns.\n");
            log_message("Rezultat: Timeout - serverul nu a raspuns in timpul alocat");
            break;
        } else {
            memset(buffer, 0, sizeof(buffer));
            int bytes_read = read(sockfd, buffer, sizeof(buffer) - 1);

            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("Raspuns server: %s\n", buffer);
                char log_msg[BUFFER_SIZE + 50];
                snprintf(log_msg, sizeof(log_msg), "Rezultat: Raspuns primit de la server - %s", buffer);
                log_message(log_msg);

                if (strncmp(buffer, "Fisierul log are", 16) == 0) {
                    printf("Introdu cate linii vrei de la final: ");
                    fgets(command, sizeof(command), stdin);
                    command[strcspn(command, "\n")] = 0;
                    write(sockfd, command, strlen(command));
                    memset(buffer, 0, sizeof(buffer));
                    read(sockfd, buffer, sizeof(buffer) - 1);
                    printf("\nUltimele linii din log:\n%s\n", buffer);
                }
            } else {
                printf("Serverul s-a deconectat.\n");
                break;
            }
        }

        if (strcmp(buffer, "LOGOUT") == 0 || strcmp(buffer, "SHUTDOWN") == 0 || strcmp(buffer, "KILL_SERVER") == 0) {
            printf("Deconectare...\n");
            break;
        }

        printf("Apasa Enter pentru a reveni la meniu...");
        getchar();
    }

    close(sockfd);
    log_message("Client admin inchis.");
    printf("\nClient admin inchis.\n");

    return 0;
}

