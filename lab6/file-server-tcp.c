#include "err.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define QUEUE_LENGTH 10
#define LINE_SIZE 100
#define BUFFER_SIZE 2500

uint64_t filesizes_sum = 0;
pthread_mutex_t filesizes_lock;

struct __attribute__((__packed__)) file_info_s {
    uint32_t filename_size;
    uint32_t file_size;
};

static void read_all(int fd, void *dest, size_t n) {
    size_t done = 0;
    while (done < n) {
        ssize_t ret = read(fd, dest + done, n - done);
        if (ret < 0) {
            perror("read_all: read");
            exit(EXIT_FAILURE);
        }
        done += ret;
    }
}

void *handle_connection(void *s_ptr) {
    char *buffer = malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        syserr("malloc");
    }

    int sock = *(int *)s_ptr;
    free(s_ptr);

    /* Get peer address and port  */
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    if (getpeername(sock, (struct sockaddr *)&client_addr, &len) == -1) {
        syserr("getsockname");
    }

    // Wątek obsługujący klienta wypisuje na standardowe wyjście:
    // new client [adres_ip_klienta:port_klienta] size=[rozmiar_pliku] file=[nazwa_pliku]
    // Następnie odczekuje 1 sekundę.
    struct file_info_s file_info;
    read_all(sock, &file_info, sizeof(file_info));
    char *filename_buff = malloc(file_info.filename_size + 1);
    filename_buff[file_info.filename_size] = 0;
    read_all(sock, filename_buff, file_info.filename_size);

    char peeraddr[LINE_SIZE + 1];
    inet_ntop(AF_INET, &client_addr.sin_addr, peeraddr, LINE_SIZE);
    printf("new client [%s:%d] size=[%d] file=[%s]\n", peeraddr, ntohs(client_addr.sin_port),
           file_info.file_size, filename_buff);

    sleep(1);

    // Odbiera plik, zapisuje go pod wskazaną nazwą

    FILE *fd = fopen(filename_buff, "wb");

    size_t bytes_read = 0;
    while (1) {
        size_t to_read = file_info.file_size - bytes_read;
        if (to_read > BUFFER_SIZE) {
            to_read = BUFFER_SIZE;
        }
        if (to_read == 0) {
            break;
        }

        ssize_t nbytes = read(sock, buffer, to_read);
        if (nbytes <= 0) {
            close(sock);
            syserr("read problem - error or unexpected end of transmission");
        }
        bytes_read += nbytes;

        size_t written_to_file = fwrite(buffer, sizeof(char), nbytes, fd);

        if (written_to_file < nbytes) {
            syserr("fwrite error");
        }
    }

    fclose(fd);

    // Wypisuje na standardowe wyjście:
    // client [adres_ip_klienta:port_klienta] has sent its file of size=[rozmiar_pliku]
    // total size of uploaded files [suma_rozmiarów_odebranych_plików]
    printf("client [%s:%d] has sent its file of size=[%i]\n", peeraddr,
           ntohs(client_addr.sin_port), file_info.file_size);

    if (pthread_mutex_lock(&filesizes_lock) != 0) {
        syserr("lock failed");
    }
    filesizes_sum += file_info.file_size;
    printf("total size of uploaded files [%lu]\n", filesizes_sum);
    if (pthread_mutex_unlock(&filesizes_lock) != 0) {
        syserr("unlock failed");
    }

    close(sock);
    free(buffer);
    return 0;
}

int main(int argc, char *argv[]) {
    // Serwer przyjmuje jeden parametr - numer portu, na którym nasłuchuje.

    if (argc < 2) {
        printf("Usage: ./client port\n");
        return 1;
    }
    uint16_t port_num = atoi(argv[1]);

    int sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock < 0) {
        syserr("socket");
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;                // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port_num);          // listening on port port_num

    // bind the socket to a concrete address
    if (bind(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        syserr("bind");
    }

    // switch to listening (passive open)
    if (listen(sock, QUEUE_LENGTH) < 0) {
        syserr("listen");
    }

    // Init filesizes_sum

    if (pthread_mutex_init(&filesizes_lock, 0) != 0) {
        syserr("mutex init failed");
    }

    // Serwer działa w pętli nieskończonej.
    // Dla każdego klienta tworzy osobny wątek.
    while (1) {
        int msgsock = accept(sock, (struct sockaddr *)NULL, NULL);
        if (msgsock == -1) {
            syserr("accept");
        }
        int *con = malloc(sizeof(int));
        *con = msgsock;

        pthread_t t;
        if (pthread_create(&t, 0, handle_connection, con) != 0) {
            syserr("pthread_create");
        }

        if (pthread_detach(t) != 0) {
            syserr("pthread_detach");
        }
    }

    return 0;
}
