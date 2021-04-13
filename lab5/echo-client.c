#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>

#include "err.h"

#define BUFFER_SIZE 70005

int main(int argc, char *argv[]) {
    int sock;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;

    if (argc < 5) {
        fatal("Usage: %s host port n k ...\n", argv[0]);
    }

    // 'converting' host/port in string to struct addrinfo
    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;
    int err = getaddrinfo(argv[1], argv[2], &addr_hints, &addr_result);
    if (err == EAI_SYSTEM) { // system error
        syserr("getaddrinfo: %s", gai_strerror(err));
    } else if (err != 0) { // other error (host not found, etc.)
        fatal("getaddrinfo: %s", gai_strerror(err));
    }

    // initialize socket according to getaddrinfo results
    sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (sock < 0)
        syserr("socket");

    // connect socket to the server
    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
        syserr("connect");

    int n = atoi(argv[3]); // liczba pakietów
    int k = atoi(argv[4]); // liczba pakietów

    freeaddrinfo(addr_result);

    char buffer[BUFFER_SIZE];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < k + 3; j += 5) {
            buffer[j + 0] = 'm';
            buffer[j + 1] = 'i';
            buffer[j + 2] = 'm';
            buffer[j + 3] = 'u';
            buffer[j + 4] = 'w';
        }

        printf("writing to socket: %i-th message\n", i);
        if (write(sock, buffer, k) != k) {
            syserr("partial / failed write");
        }
    }

    (void)close(sock); // socket would be closed anyway when the program ends

    return 0;
}
