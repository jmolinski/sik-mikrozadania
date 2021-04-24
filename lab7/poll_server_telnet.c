#include "err.h"
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024

static bool finish = false;
static int activeClients = 0;
static int activeNonTelnetClients = 0;
static int totalClients = 0;
static char buf[BUF_SIZE];
static struct pollfd client[_POSIX_OPEN_MAX];
static bool is_telnet[_POSIX_OPEN_MAX];

/* Obsługa sygnału kończenia */
static void catch_int(int sig) {
    finish = true;
    fprintf(stderr, "Signal %d catched. No new connections will be accepted.\n", sig);
}

void handle();

void set_sigint_handler() {
    struct sigaction action;
    sigset_t block_mask;
    sigemptyset(&block_mask);
    action.sa_handler = catch_int;
    action.sa_mask = block_mask;
    action.sa_flags = SA_RESTART;

    if (sigaction(SIGINT, &action, 0) == -1) {
        syserr("sigaction");
    }
}

void close_main_socket_on_sigint() {
    if (finish && client[0].fd >= 0) {
        if (close(client[0].fd) < 0) {
            syserr("close");
        }
        client[0].fd = -1;
    }
    if (finish && client[1].fd >= 0) {
        if (close(client[1].fd) < 0) {
            syserr("close");
        }
        client[1].fd = -1;
    }
}

void bind_main_sockets(uint16_t control_port, uint16_t clients_port) {
    client[0].fd = socket(PF_INET, SOCK_STREAM, 0);
    client[1].fd = socket(PF_INET, SOCK_STREAM, 0);
    if (client[0].fd == -1 || client[1].fd == -1) {
        syserr("Opening stream socket");
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(clients_port);
    if (bind(client[0].fd, (struct sockaddr *)&server, (socklen_t)sizeof(server)) == -1) {
        syserr("Binding stream socket");
    }

    struct sockaddr_in server_control;
    server_control.sin_family = AF_INET;
    server_control.sin_addr.s_addr = htonl(INADDR_ANY);
    server_control.sin_port = htons(control_port);
    if (bind(client[1].fd, (struct sockaddr *)&server_control,
             (socklen_t)sizeof(server_control)) == -1) {
        syserr("Binding stream socket");
    }

    printf("Socket port #%u\n", (unsigned)ntohs(server.sin_port));
    printf("Control server socket port #%u\n", (unsigned)ntohs(server_control.sin_port));
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: ./client port_clients control_port\n");
        return 1;
    }
    uint16_t clients_port = atoi(argv[1]);
    uint16_t control_port = atoi(argv[2]);

    fprintf(stderr, "_POSIX_OPEN_MAX = %d\n", _POSIX_OPEN_MAX);

    /* Po Ctrl-C kończymy */
    set_sigint_handler();

    /* Inicjujemy tablicę z gniazdkami klientów, client[0] to gniazdko centrali */
    for (int i = 0; i < _POSIX_OPEN_MAX; ++i) {
        client[i].fd = -1;
        client[i].events = POLLIN;
        client[i].revents = 0;
        is_telnet[i] = false;
    }

    /* Tworzymy gniazdko centrali */
    bind_main_sockets(control_port, clients_port);

    /* Zapraszamy klientów */
    if (listen(client[0].fd, 5) == -1) {
        syserr("Starting to listen");
    }
    if (listen(client[1].fd, 5) == -1) {
        syserr("Starting to listen");
    }

    /* Do pracy */
    do {
        for (int i = 0; i < _POSIX_OPEN_MAX; ++i) {
            client[i].revents = 0;
        }

        /* Po Ctrl-C zamykamy gniazdko centrali */
        close_main_socket_on_sigint();

        /* Czekamy przez 5000 ms */
        int ret = poll(client, _POSIX_OPEN_MAX, 10000);
        if (ret == -1) {
            if (errno == EINTR) {
                fprintf(stderr, "Interrupted system call\n");
            } else {
                syserr("poll");
            }
        } else if (ret > 0) {
            handle();
        } else {
            fprintf(stderr, "Do something else\n");
        }
    } while (!finish || activeClients > 0);

    if (client[0].fd >= 0) {
        if (close(client[0].fd) < 0) {
            syserr("Closing main socket");
        }
    }
    if (client[1].fd >= 0) {
        if (close(client[1].fd) < 0) {
            syserr("Closing main socket");
        }
    }

    return 0;
}

void handle_main_connection_sock() {
    if (!finish && (client[0].revents & POLLIN)) {
        int msgsock = accept(client[0].fd, (struct sockaddr *)0, (socklen_t *)0);
        if (msgsock == -1) {
            syserr("accept");
        }
        for (int i = 2; i < _POSIX_OPEN_MAX; ++i) {
            if (client[i].fd == -1) {
                fprintf(stderr, "Received new connection (%d)\n", i);
                client[i].fd = msgsock;
                client[i].events = POLLIN;
                activeClients++;
                activeNonTelnetClients++;
                totalClients++;
                break;
            }
        }
    }
}

void handle_telnet_main_connection_sock() {
    if (!finish && (client[1].revents & POLLIN)) {
        int msgsock = accept(client[1].fd, (struct sockaddr *)0, (socklen_t *)0);
        if (msgsock == -1) {
            syserr("accept");
        }
        for (int i = 2; i < _POSIX_OPEN_MAX; ++i) {
            if (client[i].fd == -1) {
                fprintf(stderr, "Received new control connection (%d)\n", i);
                client[i].fd = msgsock;
                client[i].events = POLLIN;
                activeClients += 1;
                is_telnet[i] = true;
                break;
            }
        }
    }
}

void handle_normal_clients_updates() {
    for (int i = 2; i < _POSIX_OPEN_MAX; ++i) {
        if (client[i].fd != -1 && (client[i].revents & (POLLIN | POLLERR))) {
            if (is_telnet[i]) {
                continue;
            }
            ssize_t rval = read(client[i].fd, buf, BUF_SIZE);
            if (rval < 0) {
                fprintf(stderr, "Reading message (%d, %s)\n", errno, strerror(errno));
                if (close(client[i].fd) < 0) {
                    syserr("close");
                }
                client[i].fd = -1;
                activeClients -= 1;
                activeNonTelnetClients--;
            } else if (rval == 0) {
                fprintf(stderr, "Ending connection\n");
                if (close(client[i].fd) < 0) {
                    syserr("close");
                }
                client[i].fd = -1;
                activeClients -= 1;
                activeNonTelnetClients--;
            } else {
                printf("-->%.*s\n", (int)rval, buf);
            }
        }
    }
}

void handle_telnet_clients_updates() {
    for (int i = 2; i < _POSIX_OPEN_MAX; ++i) {
        if (!(client[i].fd != -1 && (client[i].revents & (POLLIN | POLLERR)))) {
            continue;
        }
        if (!is_telnet[i]) {
            continue;
        }
        ssize_t rval = read(client[i].fd, buf, BUF_SIZE);
        if (rval < 0) {
            fprintf(stderr, "Error when reading message (%d, %s)\n", errno, strerror(errno));
        } else if (rval == 0) {
            fprintf(stderr, "Ending connection\n");
        } else if (strncmp("count", buf, 5) == 0) {
            char buffer[100];
            int written =
                snprintf(buffer, 100, "Number of active clients: %d\n", activeNonTelnetClients);
            write(client[i].fd, buffer, written);
            written = snprintf(buffer, 100, "Total number of clients: %d\n", totalClients);
            write(client[i].fd, buffer, written);
        }
        close(client[i].fd);
        client[i].fd = -1;
        is_telnet[i] = false;
        activeClients -= 1;
    }
}

void handle() {
    handle_main_connection_sock();
    handle_telnet_main_connection_sock();

    handle_normal_clients_updates();
    handle_telnet_clients_updates();
}
