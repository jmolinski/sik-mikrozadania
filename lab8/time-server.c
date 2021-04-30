#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "err.h"

#define BSIZE 1024

const char GET_TIME[10] = "GET TIME\0\0";

void *get_in_addr(struct sockaddr *sa) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fatal("Usage: %s multicast_dotted_address local_port\n", argv[0]);
    }
    char *multicast_dotted_address = argv[1];
    in_port_t local_port = atoi(argv[2]);

    /* otwarcie gniazda */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        syserr("socket");
    }

    /* podłączenie do grupy rozsyłania (ang. multicast) */
    /* zmienne i struktury opisujące gniazda */
    struct ip_mreq ip_mreq;
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0) {
        fprintf(stderr, "ERROR: inet_aton - invalid multicast address\n");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&ip_mreq, sizeof ip_mreq) < 0) {
        syserr("setsockopt");
    }

    /* ustawienie adresu i portu lokalnego */
    struct sockaddr_in local_address;
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(local_port);
    if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0) {
        syserr("bind");
    }

    char buffer[BSIZE];
    while (1) {
        memset(buffer, (unsigned)BSIZE, sizeof(char));

        ssize_t numbytes;
        struct sockaddr_storage their_addr;
        socklen_t addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sock, buffer, BSIZE - 1, 0, (struct sockaddr *)&their_addr,
                                 &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        buffer[numbytes] = '\0';

        if (numbytes != strlen(GET_TIME) || strcmp(buffer, GET_TIME) != 0) {
            printf("Received unknown command: %s\n", buffer);
            continue;
        }

        char s[INET6_ADDRSTRLEN];
        memset(s, INET6_ADDRSTRLEN, sizeof(char));
        printf("Request from: %s\n",
               inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s,
                         sizeof s));

        memset(buffer, (unsigned)BSIZE, sizeof(char));
        time_t time_buffer;
        time(&time_buffer);
        strncpy(buffer, ctime(&time_buffer), BSIZE);
        size_t length = strnlen(buffer, BSIZE);
        if (sendto(sock, buffer, length, 0, (struct sockaddr *)&their_addr, sizeof their_addr) <
            0) {
            syserr("sendto");
        }
    }
}
