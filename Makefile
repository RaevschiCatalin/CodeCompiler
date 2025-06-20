# Makefile pentru proiectul Admin Client-Server

CC = gcc
CFLAGS = -Wall -Wextra -g
SRCDIR = src
BINDIR = bin
SCRIPTSDIR = scripts

TARGETS = \
	$(BINDIR)/main \
	$(BINDIR)/admin_client \
	$(BINDIR)/client

all: $(TARGETS)

# Main executable (runs both servers)
$(BINDIR)/main: $(SCRIPTSDIR)/main.c $(SRCDIR)/server.c $(SRCDIR)/admin_server.c | $(BINDIR)
	$(CC) $(CFLAGS) -lpthread $< -o $@

# Admin client
$(BINDIR)/admin_client: $(SRCDIR)/admin_client.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@

# Regular client  
$(BINDIR)/client: $(SRCDIR)/client.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -f $(BINDIR)/* *.o *.log
	rm -rf /tmp/uploads
	rm -f /tmp/admin_socket
	rm -f /tmp/admin_server.log /tmp/blocked_ips.txt /tmp/uploads_info.txt /tmp/dummy_raport.xlsx raport_primire.xlsx upload_log_primit.txt
	rm -f hello.py error.c runtime_error.py sleep_test.py result_hello.txt result_error.txt result_runtime.txt result_sleep1.txt result_sleep2.txt result_sleep3.txt result_sleep4.txt raport_primire.xlsx

.PHONY: all clean
