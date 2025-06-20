#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/file.h>

#define PORT 12345
#define BACKLOG 10
#define MAX_FILENAME 256
#define MAX_FILESIZE (1024 * 1024)
#define MAX_JOBS 100
#define MAX_WORKERS 3
#define MAX_CLIENTS 1024


int is_ip_blocked(const char *ip) {
    FILE *fp = fopen("/tmp/blocked_ips.txt", "r");
    if (!fp) return 0;

    char line[64];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0; 
	if (strcmp(line, ip) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}


typedef struct {
    int client_sock;
    char header[512];
} Job;

Job job_queue[MAX_JOBS];
int job_front = 0, job_rear = 0, job_count = 0;

pthread_mutex_t job_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_cond = PTHREAD_COND_INITIALIZER;
void *handle_client(void *arg);
volatile sig_atomic_t running = 1;

int server_sock = -1;
int client_socks[MAX_CLIENTS];
int client_count = 0;

void cleanup() {
    if (server_sock != -1) close(server_sock);
    for (int i = 0; i < client_count; ++i) {
        if (client_socks[i] != -1) close(client_socks[i]);
    }
}

void handle_signal(int signo) {
    (void)signo;
    printf("\n[!] Server shutting down (caught signal).\n");
    cleanup();
    exit(0);
}

void sanitize_filename(char *filename) {
    for (int i = 0; filename[i]; ++i) {
        if (!isalnum(filename[i]) && filename[i] != '.' && filename[i] != '_' && filename[i] != '-') {
            filename[i] = '_';
        }
    }
}

const char *get_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    return dot ? dot + 1 : "";
}

int run_command(const char *cmd, const char *outfile) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s > %s.stdout 2> %s.stderr", cmd, outfile, outfile);
    int ret = system(buf);
    return WEXITSTATUS(ret);
}

ssize_t recv_line(int sock, char *buf, size_t maxlen) {
    size_t i = 0;
    char c = 0;
    while (i < maxlen - 1) {
        ssize_t n = recv(sock, &c, 1, 0);
        if (n <= 0) return n;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}

void enqueue_job(int client_sock) {
    pthread_mutex_lock(&job_mutex);
    if (job_count < MAX_JOBS) {
        job_queue[job_rear].client_sock = client_sock;
        job_rear = (job_rear + 1) % MAX_JOBS;
        job_count++;
        pthread_cond_signal(&job_cond);
    } else {
        printf("[!] Job queue full! Dropping job.\n");
        close(client_sock);
    }
    pthread_mutex_unlock(&job_mutex);
}

void *worker_thread(void *arg) {
    while (running) {
        pthread_mutex_lock(&job_mutex);
        while (job_count == 0 && running) {
            pthread_cond_wait(&job_cond, &job_mutex);
        }
        if (!running) {
            pthread_mutex_unlock(&job_mutex);
            break;
        }
        Job job = job_queue[job_front];
        job_front = (job_front + 1) % MAX_JOBS;
        job_count--;
        pthread_mutex_unlock(&job_mutex);
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = job.client_sock;
        handle_client((void *)sock_ptr);
    }
    return NULL;
}

void update_connected_clients(const char *ip, int add) {
    FILE *fp = fopen("/tmp/connected_clients.txt", "r+");
    if (!fp) fp = fopen("/tmp/connected_clients.txt", "w+");
    if (!fp) return;
    flock(fileno(fp), LOCK_EX);
    char ips[MAX_CLIENTS][64];
    int count = 0, found = 0;
    char buf[64];
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\n")] = 0;
        if (strcmp(buf, ip) == 0) found = 1;
        else if (count < MAX_CLIENTS) strcpy(ips[count++], buf);
    }
    rewind(fp);
    if (add && !found) strcpy(ips[count++], ip);
    if (!add && found) {
        int j = 0;
        for (int i = 0; i < count; ++i) if (strcmp(ips[i], ip) != 0) strcpy(ips[j++], ips[i]);
        count = j;
    }
    for (int i = 0; i < count; ++i) fprintf(fp, "%s\n", ips[i]);
    ftruncate(fileno(fp), ftell(fp));
    flock(fileno(fp), LOCK_UN);
    fclose(fp);
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    client_socks[client_count++] = client_sock;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(client_sock, (struct sockaddr *)&addr, &len);
    char ipbuf[64];
    inet_ntop(AF_INET, &addr.sin_addr, ipbuf, sizeof(ipbuf));
    printf("[+] Client connected. Socket: %d\n", client_sock);
    char header[512];
    ssize_t n = recv_line(client_sock, header, sizeof(header));
    if (n <= 0) {
        printf("[!] Failed to read header line\n");
        close(client_sock);
        return NULL;
    }
    printf("[LOG] Received header: %s", header);
    if (strncmp(header, "SHUTDOWN", 8) == 0) {
        printf("[ADMIN] Received shutdown command.\n");
        const char *msg = "Server shutting down.\n";
        send(client_sock, msg, strlen(msg), 0);
        running = 0;
        close(client_sock);
        return NULL;
    }
    char cmd[16], filename[MAX_FILENAME];
    int filesize = 0;
    if (sscanf(header, "%15s %255s %d", cmd, filename, &filesize) != 3 || strcmp(cmd, "UPLOAD") != 0) {
        printf("[!] Invalid upload command\n");
        close(client_sock);
        return NULL;
    }
    if (filesize <= 0 || filesize > MAX_FILESIZE) {
        printf("[!] Invalid file size: %d\n", filesize);
        close(client_sock);
        return NULL;
    }
    sanitize_filename(filename);
    printf("[>] Receiving file: %s (%d bytes)\n", filename, filesize);
    char filepath[300];
    snprintf(filepath, sizeof(filepath), "/tmp/%d_%s", client_sock, filename);
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        printf("[!] Failed to open file for writing\n");
        close(client_sock);
        return NULL;
    }
    int bytes_left = filesize;
    char buf[4096];
    while (bytes_left > 0) {
        int to_read = bytes_left > sizeof(buf) ? sizeof(buf) : bytes_left;
        int r = recv(client_sock, buf, to_read, 0);
        if (r <= 0) {
            printf("[!] File receive error\n");
            fclose(fp);
            close(client_sock);
            return NULL;
        }
        fwrite(buf, 1, r, fp);
        bytes_left -= r;
    }
    fclose(fp);
    printf("[>] File received and saved to %s\n", filepath);
    char run_cmd[16] = {0};
    n = recv_line(client_sock, run_cmd, sizeof(run_cmd));
    if (n <= 0 || strncmp(run_cmd, "RUN", 3) != 0) {
        printf("[!] Invalid or missing RUN command\n");
        close(client_sock);
        return NULL;
    }
    printf("[>] RUN command received.\n");
    const char *ext = get_ext(filename);
    char compile_cmd[512] = "", exec_cmd[512] = "", resultfile[300];
    int is_compiled = 0;
    snprintf(resultfile, sizeof(resultfile), "/tmp/%d_%s.result", client_sock, filename);
    int exit_code = 0;
    if (strcmp(ext, "c") == 0) {
        snprintf(compile_cmd, sizeof(compile_cmd), "gcc %s -o /tmp/%s.bin", filepath, filename);
        snprintf(exec_cmd, sizeof(exec_cmd), "/tmp/%s.bin", filename);
        is_compiled = 1;
    } else if (strcmp(ext, "cpp") == 0) {
        snprintf(compile_cmd, sizeof(compile_cmd), "g++ %s -o /tmp/%s.bin", filepath, filename);
        snprintf(exec_cmd, sizeof(exec_cmd), "/tmp/%s.bin", filename);
        is_compiled = 1;
    } else if (strcmp(ext, "py") == 0) {
        snprintf(exec_cmd, sizeof(exec_cmd), "python3 %s", filepath);
    } else if (strcmp(ext, "java") == 0) {
        char classname[128] = "";
        sscanf(filename, "%127[^.]", classname);
        snprintf(compile_cmd, sizeof(compile_cmd), "javac %s", filepath);
        snprintf(exec_cmd, sizeof(exec_cmd), "java -cp /tmp %s", classname);
        is_compiled = 1;
    } else {
        const char *msg = "ERROR: Unsupported file type\n";
        send(client_sock, msg, strlen(msg), 0);
        close(client_sock);
        return NULL;
    }
    if (is_compiled) {
        printf("[>] Compiling: %s\n", compile_cmd);
        exit_code = run_command(compile_cmd, resultfile);
        if (exit_code != 0) {
            const char *msg = "ERROR: Compilation failed\n";
            send(client_sock, msg, strlen(msg), 0);
            close(client_sock);
            return NULL;
        }
    }
    printf("[>] Executing: %s\n", exec_cmd);
    exit_code = run_command(exec_cmd, resultfile);
    if (exit_code != 0) {
        const char *msg = "ERROR: Execution failed\n";
        send(client_sock, msg, strlen(msg), 0);
        close(client_sock);
        return NULL;
    }
    char stdoutfile[320], stderrfile[320];
    snprintf(stdoutfile, sizeof(stdoutfile), "%s.stdout", resultfile);
    snprintf(stderrfile, sizeof(stderrfile), "%s.stderr", resultfile);
    FILE *rf = fopen(resultfile, "wb");
    if (!rf) {
        printf("[!] Could not open result file for writing\n");
        close(client_sock);
        return NULL;
    }
    FILE *sf = fopen(stdoutfile, "rb");
    if (sf) {
        fprintf(rf, "--- STDOUT ---\n");
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), sf)) > 0) fwrite(buf, 1, r, rf);
        fclose(sf);
    } else {
        fprintf(rf, "--- STDOUT ---\n<none>\n");
    }
    FILE *ef = fopen(stderrfile, "rb");
    if (ef) {
        fprintf(rf, "\n--- STDERR ---\n");
        size_t r;
        while ((r = fread(buf, 1, sizeof(buf), ef)) > 0) fwrite(buf, 1, r, rf);
        fclose(ef);
    } else {
        fprintf(rf, "\n--- STDERR ---\n<none>\n");
    }
    fprintf(rf, "\n--- EXIT CODE ---\n%d\n", exit_code);
    fclose(rf);
    rf = fopen(resultfile, "rb");
    if (!rf) {
        printf("[!] Could not open result file for sending\n");
        close(client_sock);
        return NULL;
    }
    fseek(rf, 0, SEEK_END);
    long result_size = ftell(rf);
    fseek(rf, 0, SEEK_SET);
    char sizebuf[64];
    snprintf(sizebuf, sizeof(sizebuf), "%ld\n", result_size);
    send(client_sock, sizebuf, strlen(sizebuf), 0);
    while ((n = fread(buf, 1, sizeof(buf), rf)) > 0) {
        send(client_sock, buf, n, 0);
    }
    fclose(rf);
    close(client_sock);
    printf("[-] Client disconnected. Socket: %d\n", client_sock);
    update_connected_clients(ipbuf, 0);
    // Remove from client_socks
    for (int i = 0; i < client_count; ++i) {
        if (client_socks[i] == client_sock) {
            client_socks[i] = -1;
            break;
        }
    }
    return NULL;
}

int server_main() {
    int server_sock, *client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    pthread_t tid;
    pthread_t workers[MAX_WORKERS];
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    memset(&(server_addr.sin_zero), '\0', 8);
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    if (listen(server_sock, BACKLOG) < 0) {
        perror("listen");
        close(server_sock);
        exit(EXIT_FAILURE);
    }
    printf("[+] Server started on port %d. Waiting for clients...\n", PORT);
    for (int i = 0; i < MAX_WORKERS; ++i) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }
    while (running) {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) {
            if (errno == EINTR && !running) break;
            perror("accept");
            continue;
        }
	        char ipbuf[64];
        inet_ntop(AF_INET, &client_addr.sin_addr, ipbuf, sizeof(ipbuf));

        // 🔒 verificare dacă IP-ul este blocat
        FILE *fp = fopen("/tmp/blocked_ips.txt", "r");
        int is_blocked = 0;
        if (fp) {
            char line[64];
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\n")] = 0;
                if (strcmp(line, ipbuf) == 0) {
                    is_blocked = 1;
                    break;
                }
            }
            fclose(fp);
        }

        if (is_blocked) {
            printf("[!] Refused blocked IP: %s\n", ipbuf);
            const char *msg = "Your IP is blocked.\n";
            send(client_sock, msg, strlen(msg), 0); // optional
            close(client_sock);
            continue;
        }

        update_connected_clients(ipbuf, 1);
        int *sock_ptr = malloc(sizeof(int));
        *sock_ptr = client_sock;
        pthread_create(&tid, NULL, handle_client, sock_ptr);
        pthread_detach(tid);

   }
    cleanup();
    remove("/tmp/main_server.pid");
    remove("/tmp/connected_clients.txt");
    printf("[!] Server shutting down...\n");
    pthread_cond_broadcast(&job_cond);
    for (int i = 0; i < MAX_WORKERS; ++i) {
        pthread_join(workers[i], NULL);
    }
    close(server_sock);
    return 0;
} 
