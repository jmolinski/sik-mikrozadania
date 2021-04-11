#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "err.h"

#define BUFFER_SIZE 1000
#define PORT_NUM 10001

int main(int argc, char *argv[]) {
    int sock;
    int flags;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;

    char buffer[BUFFER_SIZE];
    socklen_t rcva_len;
    ssize_t len;

    sock = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
    if (sock < 0)
        syserr("socket");
    // after socket() call; we should close(sock) on any execution path;

    server_address.sin_family = AF_INET;                // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(PORT_NUM);          // default port for receiving is PORT_NUM

    // bind the socket to a concrete address
    if (bind(sock, (struct sockaddr *)&server_address, (socklen_t)sizeof(server_address)) < 0) {
        syserr("bind");
    }

    FILE *fptr = fopen("ustalona_nazwa.txt", "wb+");
    fflush(fptr);

    for (;;) {
        do {
            for (int j = 0; j < BUFFER_SIZE; j++) {
                buffer[j] = 0;
            }

            rcva_len = (socklen_t)sizeof(client_address);
            len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_address,
                           &rcva_len);
            if (len < 0) {
                syserr("error on datagram from client socket");
            } else {
                (void)printf("%zd bytes\n", len);
                fwrite(buffer, 1, len, fptr);
                fwrite("\n", 1, 1, fptr);
                fflush(fptr);
            }
        } while (len > 0);

        (void)printf("finished exchange\n");
    }

    if (close(sock) == -1) { // very rare errors can occur here, but then
        syserr("close");     // it's healthy to do the check
    }

    return 0;
}
