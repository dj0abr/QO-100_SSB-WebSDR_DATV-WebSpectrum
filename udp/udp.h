#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct {
    int *sock;
    int port;
    void (*rxfunc)(uint8_t *, int, struct sockaddr_in*);
    int *stopped;
} RXCFG;

void UdpRxInit(int *sock, int port, void (*rxfunc)(uint8_t *, int, struct sockaddr_in*), int *stopped);
void sendUDP(char *destIP, int destPort, uint8_t *pdata, int len);
