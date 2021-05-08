/*
* super simple UDP handler
* ========================
* Author: DJ0ABR
*
*   (c) DJ0ABR
*   www.dj0abr.de
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
* 
*/

#include "udp.h"

void* threadfunction(void* param);

#define MAXUDPTHREADS 20
RXCFG udprxcfg[MAXUDPTHREADS];
int rxcfg_idx = 0;

// start UDP reception
// sock ... pointer to a socket (just a pointer to an int)
// port ... own port, messages only to this port are received
// rxfunc ... pointer to a callback function, will be called for received data
// stopped ... pointer to an int. If it is set to 1, the function exits
void UdpRxInit(int *sock, int port, void (*rxfunc)(uint8_t *, int, struct sockaddr_in*), int *stopped)
{
    if(rxcfg_idx >= MAXUDPTHREADS)
    {
        printf("max number of UDP threads\n");
        exit(0);
    }
    
    udprxcfg[rxcfg_idx].sock = sock;
    udprxcfg[rxcfg_idx].port = port;
    udprxcfg[rxcfg_idx].rxfunc = rxfunc;
    udprxcfg[rxcfg_idx].stopped = stopped;
    
    // bind port
    struct sockaddr_in sin;


	*sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (*sock == -1){
		printf("Failed to create Socket\n");
		exit(0);
	}
	
	char enable = 1;
	setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
    sin.sin_addr.s_addr = INADDR_ANY;

	if (bind(*sock, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) != 0)
	{
		printf("Failed to bind socket, port:%d\n",port);
        close(*sock);
		exit(0);
	}

    //printf("port %d sucessfully bound\n", port);
	
	// port sucessfully bound
	// create the receive thread
	pthread_t rxthread;
	pthread_create(&rxthread, NULL, threadfunction, &(udprxcfg[rxcfg_idx]));
    rxcfg_idx++;
}

void* threadfunction(void* param) 
{
    socklen_t fromlen;
    pthread_detach(pthread_self());
    RXCFG rxcfg;
    memcpy((uint8_t *)(&rxcfg), (uint8_t *)param, sizeof(RXCFG));
	int recvlen;
    const int maxUDPpacketsize = 100000;
	char rxbuf[maxUDPpacketsize];
	struct sockaddr_in fromSock;
	fromlen = sizeof(struct sockaddr_in);
	while(*rxcfg.stopped == 0)
	{
        recvlen = recvfrom(*rxcfg.sock, rxbuf, maxUDPpacketsize, 0, (struct sockaddr *)&fromSock, &fromlen);
		if (recvlen > 0)
        {
            // data received, send it to callback function
            (*rxcfg.rxfunc)((uint8_t *)rxbuf,recvlen, &fromSock);
        }
        if (recvlen < 0)
        {
        }
	}
    pthread_exit(NULL); // self terminate this thread
    return NULL;
}

// send UDP message
// this function can always be used without any initialisation
void sendUDP(char *destIP, int destPort, uint8_t *pdata, int len)
{
    int sockfd; 
    struct sockaddr_in     servaddr; 
  
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        printf("sendUDP: socket creation failed\n"); 
        exit(0); 
    } 
    memset(&servaddr, 0, sizeof(servaddr)); 
    // Filling server information 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(destPort); 
    //printf("Send to <%s><%d> Len:%d\n",destIP,destPort,len);
    servaddr.sin_addr.s_addr=inet_addr(destIP);
    sendto(sockfd, (char *)pdata, len, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)); 
    close(sockfd);
}
