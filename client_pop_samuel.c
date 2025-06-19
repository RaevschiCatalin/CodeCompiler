#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>

void usage(const char *prog) {
    printf("Usage: %s --file <path> --output <save_as>\n", prog);
    exit(1);
}

int main(int argc, char *argv[]) {
    char *filepath = NULL, *outputpath = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            filepath = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputpath = argv[++i];
        }
    }
    if (!filepath || !outputpath) usage(argv[0]);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) { perror("fopen"); return 1; }
    struct stat st;
    if (stat(filepath, &st) < 0) { perror("stat"); fclose(fp); return 1; }
    int filesize = st.st_size;
    char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : (char *)filepath;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); return 1;
    }
    printf("Connected to server!\n");

    char header[512];
    snprintf(header, sizeof(header), "UPLOAD %s %d\n", filename, filesize);
    send(sock, header, strlen(header), 0);

    char buf[4096];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        send(sock, buf, n, 0);
    }
    fclose(fp);
    printf("[>] File sent.\n");

    send(sock, "RUN\n", 4, 0);
    printf("[>] RUN command sent.\n");

    // ============================ modificare Samuel Pop ============================
    char response[512] = {0};
    int r = recv(sock, response, sizeof(response) - 1, 0);
    if (r <= 0) {
        perror("[!] Failed to receive server response");
        close(sock);
        return 1;
    }
    response[r] = '\0';
    if (strncmp(response, "ERROR:", 6) == 0) {
        printf("[!] Server responded with error: %s", response); // modificare Samuel Pop
        close(sock);
        return 1;
    }
    // ==============================================================================

    // Dacă nu e eroare, presupunem că e dimensiunea fișierului rezultat
    char sizebuf[64] = {0};
    int idx = 0;
    char c;
    // Copiem primul caracter deja citit (deja e în `response`)
    for (idx = 0; idx < sizeof(sizebuf) - 1 && response[idx] != '\0' && response[idx] != '\n'; ++idx) {
        sizebuf[idx] = response[idx];
    }
    if (response[idx] == '\n') sizebuf[idx] = '\0';
    else {
        while (idx < sizeof(sizebuf) - 1) {
            if (recv(sock, &c, 1, 0) <= 0) { perror("recv"); close(sock); return 1; }
            if (c == '\n') break;
            sizebuf[idx++] = c;
        }
        sizebuf[idx] = '\0';
    }

    long result_size = atol(sizebuf);
    printf("[<] Result file size: %ld\n", result_size);

    FILE *outf = fopen(outputpath, "wb");
    if (!outf) { perror("fopen output"); close(sock); return 1; }
    long bytes_left = result_size;
    while (bytes_left > 0) {
        int to_read = bytes_left > sizeof(buf) ? sizeof(buf) : bytes_left;
        n = recv(sock, buf, to_read, 0);
        if (n <= 0) { perror("recv result"); break; }
        fwrite(buf, 1, n, outf);
        bytes_left -= n;
    }
    fclose(outf);
    printf("[<] Result file saved to %s\n", outputpath);

    close(sock);
    printf("Disconnected from server\n");
    return 0;
}

