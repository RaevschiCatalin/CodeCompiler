#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main() {
    // Create a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Socket creation error\n");
        return 1;
    }
    
    // Set up server address
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);  // Server port
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // localhost
    
    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Connection failed\n");
        return 1;
    }
    
    printf("Connected to server!\n");
    
    // Send a message
    char message[100];
    printf("Enter message to send: ");
    fgets(message, 100, stdin);
    send(sock, message, strlen(message), 0);
    
    // Receive response
    char buffer[100] = {0};
    recv(sock, buffer, 100, 0);
    printf("Server says: %s\n", buffer);
    
    // Close socket
    close(sock);
    printf("Disconnected from server\n");
    
    return 0;
}
