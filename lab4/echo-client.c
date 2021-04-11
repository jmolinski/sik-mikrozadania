#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "err.h"

/*
Zmodyfikuj klienta w ten sposób, aby przyjmował dwa dodatkowe parametry: liczbę pakietów do
wysłania (n) i rozmiar porcji danych (k). Klient n razy przesyła k dowolnych bajtów do serwera. Do
skonstruowania takiego komunikatu możesz użyć funkcji memset().


Zmodyfikuj serwer w ten sposób, aby czytał w pętli komunikaty od klienta.
 Po otrzymaniu danych serwer dopisuje je do pliku o ustalonej nazwie,
 a na standardowe wyjście podaje jedynie liczbę otrzymanych bajtów.

Przetestuj różne wartości k (np. 10, 100, 1000, 5000).
 Zaobserwuj, czy wartości wypisywane przez serwer zależą od parametru klienta.
 Uruchom klienta i serwera na dwóch różnych maszynach.

Rozwiązania można prezentować w trakcie zajęć nr 4 lub 5.
 */

int main(int argc, char *argv[]) {
    int sock;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;

    int sflags;

    size_t len;
    ssize_t snd_len, rcv_len;
    struct sockaddr_in my_address;
    struct sockaddr_in srvr_address;
    socklen_t rcva_len;

    if (argc < 3) {
        fatal("Usage: %s host port liczba pakietuw (n) rozmiar porcji (k) \n", argv[0]);
    }

    // 'converting' host/port in string to struct addrinfo
    (void)memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    addr_hints.ai_flags = 0;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;
    if (getaddrinfo(argv[1], NULL, &addr_hints, &addr_result) != 0) {
        syserr("getaddrinfo");
    }

    my_address.sin_family = AF_INET; // IPv4
    my_address.sin_addr.s_addr =
        ((struct sockaddr_in *)(addr_result->ai_addr))->sin_addr.s_addr; // address IP
    my_address.sin_port = htons((uint16_t)atoi(argv[2])); // port from the command line

    int n = atoi(argv[3]); // liczba pakietów
    int k = atoi(argv[4]); // liczba pakietów

    unsigned BUFFER_SIZE = k + 10;
    char *buffer = malloc(BUFFER_SIZE);
    char *secondary_buffer = malloc(BUFFER_SIZE);

    freeaddrinfo(addr_result);

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        syserr("socket");
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < k + 3; j += 5) {
            buffer[j + 0] = 'm';
            buffer[j + 1] = 'i';
            buffer[j + 2] = 'm';
            buffer[j + 3] = 'u';
            buffer[j + 4] = 'w';
        }

        (void)printf("sending to socket: msg of size %d\n", k);
        sflags = 0;
        rcva_len = (socklen_t)sizeof(my_address);
        len = k;
        snd_len = sendto(sock, buffer, len, sflags, (struct sockaddr *)&my_address, rcva_len);
        if (snd_len != (ssize_t)len) {
            syserr("partial / failed write");
        }
    }

    if (close(sock) == -1) { // very rare errors can occur here, but then
        syserr("close");     // it's healthy to do the check
    }

    free(buffer);
    free(secondary_buffer);

    return 0;
}
