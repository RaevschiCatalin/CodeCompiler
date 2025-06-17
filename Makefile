# Makefile pentru proiectul Admin Client-Server

CC = gcc
CFLAGS = -Wall -Wextra -g
TARGETS = admin_client admin_server client server

all: $(TARGETS)

admin_client: admin_client.c
	$(CC) $(CFLAGS) admin_client.c -o admin_client

admin_server: admin_server.c
	$(CC) $(CFLAGS) admin_server.c -o admin_server

server: server.c
	$(CC) $(CFLAGS) -lpthread server.c -o server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f $(TARGETS) *.o *.log
	rm -rf /tmp/uploads
	rm -f /tmp/admin_socket
	rm -f /tmp/admin_server.log /tmp/blocked_ips.txt /tmp/uploads_info.txt /tmp/dummy_raport.xlsx raport_primire.xlsx upload_log_primit.txt
	
