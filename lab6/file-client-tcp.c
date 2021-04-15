#include "err.h"
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 4000
char buffer[BUFFER_SIZE];

uint32_t get_file_size(char *filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

struct __attribute__((__packed__)) file_info_s {
    uint32_t filename_size;
    uint32_t file_size;
};

int main(int argc, char *argv[]) {
    // Klient przyjmuje trzy parametry -
    // adres ip lub nazwę serwera, port serwera i nazwę pliku do przesłania.
    if (argc < 4) {
        printf("Usage: ./client ip port filename\n");
        return 1;
    }

    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        syserr("socket");
    }

    /* Trzeba się dowiedzieć o adres internetowy serwera. */
    struct addrinfo addr_hints, *addr_result;
    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_flags = 0;
    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    int rc = getaddrinfo(argv[1], argv[2], &addr_hints, &addr_result);
    if (rc != 0) {
        fprintf(stderr, "rc=%d\n", rc);
        syserr("getaddrinfo: %s", gai_strerror(rc));
    }

    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) != 0) {
        syserr("connect");
    }
    freeaddrinfo(addr_result);

    // CONNECTED

    // Klient łączy się z serwerem, przesyła mu nazwę pliku i jego rozmiar, a następnie przesyła
    // plik i kończy działanie.

    struct file_info_s file_info = {.filename_size = strlen(argv[3]),
                                    .file_size = get_file_size(argv[3])};

    write(sock, &file_info, sizeof(file_info));
    write(sock, argv[3], file_info.filename_size); // Note: brak nulla na końcu.

    FILE *fd = fopen(argv[3], "rb");

    size_t bytes_read = 0;
    while (1) {
        size_t to_read = file_info.file_size - bytes_read;
        if (to_read > BUFFER_SIZE) {
            to_read = BUFFER_SIZE;
        }
        if (to_read == 0) {
            break;
        }

        size_t nbytes = fread(buffer, sizeof(char), BUFFER_SIZE, fd);
        if (nbytes != to_read) {
            close(sock);
            syserr("read problem");
        }

        bytes_read += to_read;

        if (write(sock, buffer, to_read) < 0) {
            syserr("writing on stream socket");
        }
    }

    fclose(fd);

    if (close(sock) < 0) {
        syserr("closing stream socket");
    }

    return 0;
}
