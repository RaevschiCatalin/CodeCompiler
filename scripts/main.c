#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

//  server files
#include "../src/server.c"
#include "../src/admin_server.c"


pthread_t server_thread;
pthread_t admin_thread;

void cleanup() {
    printf("\nShutting down servers...\n");
    
    pthread_cancel(server_thread);
    pthread_cancel(admin_thread);
    
    pthread_join(server_thread, NULL);
    pthread_join(admin_thread, NULL);
    
    unlink("/tmp/admin_socket");
    printf("All servers stopped.\n");
}

void handle_signal(int sig) {
    cleanup();
    exit(0);
}

void* run_server(void* arg) {
    server_main();
    return NULL;
}

void* run_admin(void* arg) {
    admin_main();
    return NULL;
}

int main() {
    printf("Starting Code Compiler Servers...\n");
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    pthread_create(&server_thread, NULL, run_server, NULL);
    printf("Main server thread started\n");
    
    pthread_create(&admin_thread, NULL, run_admin, NULL);
    printf("Admin server thread started\n");
    
    printf("Both servers running. Press Ctrl+C to stop.\n");
    
    pthread_join(server_thread, NULL);
    pthread_join(admin_thread, NULL);
    
    return 0;
}
