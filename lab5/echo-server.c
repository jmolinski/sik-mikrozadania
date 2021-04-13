#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "err.h"

#define BUFFER_SIZE 70005
#define QUEUE_LENGTH 5

int main(int argc, char *argv[]) {
    int sock, msg_sock;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;

    int port_num = 10347;

    char buffer[BUFFER_SIZE];
    ssize_t len, snd_len;

    sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock < 0)
        syserr("socket");
    // after socket() call; we should close(sock) on any execution path;
    // since all execution paths exit immediately, sock would be closed when program terminates

    server_address.sin_family = AF_INET;                // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port_num);          // listening on port port_num

    // bind the socket to a concrete address
    if (bind(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        syserr("bind");

    // switch to listening (passive open)
    if (listen(sock, QUEUE_LENGTH) < 0)
        syserr("listen");

    FILE *fptr = fopen("ustalona_nazwa.txt", "wb+");
    fflush(fptr);

    printf("accepting client connections on port %hu\n", ntohs(server_address.sin_port));
    while(1) {
        client_address_len = sizeof(client_address);
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *)&client_address, &client_address_len);
        if (msg_sock < 0)
            syserr("accept");
        do {
            len = read(msg_sock, buffer, sizeof(buffer));
            if (len < 0)
                syserr("reading from client socket");
            else {
                printf("read from socket: %zd bytes\n", len);

                fwrite(buffer, 1, len, fptr);
                fflush(fptr);
            }
        } while (len > 0);
        printf("ending connection\n");
        if (close(msg_sock) < 0)
            syserr("close");
    }

    return 0;
}
