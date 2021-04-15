#include "err.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

struct __attribute__((__packed__)) file_info {
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

int main(int argc, char *argv[]) {
    // Serwer przyjmuje jeden parametr - numer portu, na którym nasłuchuje.
    // Serwer działa w pętli nieskończonej.
    // Dla każdego klienta tworzy osobny wątek. Wątek obsługujący klienta wypisuje na standardowe
    // wyjście:
    //
    // new client [adres_ip_klienta:port_klienta] size=[rozmiar_pliku] file=[nazwa_pliku]
    // Następnie odczekuje 1 sekundę, odbiera plik, zapisuje go pod wskazaną nazwą i wypisuje na
    // standardowe wyjście:
    //
    // client [adres_ip_klienta:port_klienta] has sent its file of size=[rozmiar_pliku]
    // total size of uploaded files [suma_rozmiarów_odebranych_plików]
    // Po czym wątek kończy działanie.
}