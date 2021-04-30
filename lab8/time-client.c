#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "err.h"

#define BSIZE 256
#define TTL_VALUE 4

const char GET_TIME[10] = "GET TIME\0\0";

void setSockOptions(int sock, int sock2) {
    /* uaktywnienie rozgłaszania (ang. broadcast) */
    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof optval) < 0) {
        syserr("setsockopt broadcast");
    }
    if (setsockopt(sock2, SOL_SOCKET, SO_BROADCAST, (void *)&optval, sizeof optval) < 0) {
        syserr("setsockopt broadcast");
    }

    /* ustawienie TTL dla datagramów rozsyłanych do grupy */
    optval = TTL_VALUE;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&optval, sizeof optval) < 0) {
        syserr("setsockopt multicast ttl");
    }
}

void *get_in_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fatal("Usage: %s remote_address remote_port\n", argv[0]);
    }
    char *remote_dotted_address = argv[1];
    in_port_t remote_port = atoi(argv[2]);

    /* otwarcie gniazda */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        syserr("socket");
    }
    /* otwarcie gniazda */
    int listen_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        syserr("socket");
    }

    setSockOptions(sock, listen_sock);

    /* ustawienie adresu i portu odbiorcy */
    struct sockaddr_in remote_address;
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(remote_port);
    if (inet_aton(remote_dotted_address, &remote_address.sin_addr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }

    struct pollfd pfds[1]; // More if you want to monitor more
    memset(pfds, 1, sizeof pfds);
    pfds[0].fd = sock;       // Standard input
    pfds[0].events = POLLIN; // Tell me when ready to read

    bool success = false;
    char buffer[BSIZE];
    for (int i = 1; i <= 3; i++) {
        if (sendto(sock, GET_TIME, strlen(GET_TIME), 0, (struct sockaddr *)&remote_address,
                   sizeof remote_address) < 0) {
            syserr("sendto");
        }
        printf("Sending request [%d]\n", i);

        int num_events = poll(pfds, 1, 3000);

        if (num_events == 0) {
            continue;
        }

        if (pfds[0].revents & POLLIN) {
            ssize_t numbytes;
            struct sockaddr_storage their_addr;
            socklen_t addr_len = sizeof their_addr;
            if ((numbytes = recvfrom(sock, buffer, BSIZE - 1, 0, (struct sockaddr *)&their_addr,
                                     &addr_len)) == -1) {
                perror("recvfrom");
                exit(1);
            }
            buffer[numbytes] = '\0';
            printf("\nReceived time: %s\n", buffer);

            char s[INET6_ADDRSTRLEN];
            memset(s, INET6_ADDRSTRLEN, sizeof(char));
            printf("Response from: %s\n",
                   inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s,
                             sizeof s));

            success = true;
            break;
        }
    }

    if (!success) {
        printf("Timeout: unable to receive response.\n");
    }

    close(sock);
    close(listen_sock);
    exit(EXIT_SUCCESS);
}
