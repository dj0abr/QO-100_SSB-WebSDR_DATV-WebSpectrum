/*
* Web based SDR Client for SDRplay
* =============================================================
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
* ws_callbacks.c ... callback functions of the Websocket Server
* 
 * Websocket Server: Callback functions
 * each client get one thread and these callback functions
 * Clients are identified by fd
* 
* This WebSocketServer is a multi threaded implementation and
* opens a new thread for each client
* 
*/

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "websocketserver.h"
#include "../minitiouner.h"
#include "../setqrg.h"
#include "../fifo.h"

void get_ownIP();

extern int useCAT;
int connections = 0;
char myIP[20];

// a new browser connected
void onopen(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
    if(cli != NULL)
    {
        insert_socket(fd,cli);
        printf("Connection opened, client: %d | addr: %s\n", fd, cli);
        connections++;
        printf("%d users logged in\n",connections);
        free(cli);
    }
}

// a browser disconnected
void onclose(int fd)
{
    remove_socket(fd);
    close(fd);
    printf("Connection closed, client: %d\n", fd);
    connections--;
    printf("%d users logged in\n",connections);
}

// test if a cli (IP) is already logged in
// 1=yes, 0=no
int check_address(int fd)
{
char *cli = ws_getaddress(fd); // get IP address

    int res = test_socket(cli);
    free(cli);
    
    return res;
}

// if avaiable, send data to a browser
int onwork(int fd, unsigned char *cnt0, unsigned char *cnt1)
{
int ret = 0;

    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket == fd)
        {
            // send all available data in one frame
            // (sending multiple frames resulted in data loss)
            int len;
            unsigned char *p = ws_build_txframe(i,&len);
            if(p != NULL)
            {
                ret = ws_sendframe_binary(fd, p, len);
                free(p);
                actsock[i].send0 = 0;
                actsock[i].send1 = 0;
            }
            return ret;
        }
    }
    return 0;
}

// received a Websocket Message from a browser
void onmessage(int fd, unsigned char *msg)
{
int remoteaccess = 0;
static int f=1;

    if(f == 1)
    {
        // init own IP
        f=0;
        get_ownIP();
    }

	char *cli = ws_getaddress(fd);
    if(cli != NULL)
    {
        fclient = get_socket_idx(fd);
        
        // check if IP is authorized to control the SDRplay
        // allow only internal computers
        if(memcmp(cli,myIP,strlen(myIP)))
        {
            remoteaccess = 1;
        }
        
        
        if(useCAT && remoteaccess)
        {
            printf("ignore remote access %s for %s\n",msg, cli);
            free(cli);
            return;
        }
        
        printf("user message: %s, from: %s/%d\n", msg, cli, fd);
        
        if(strstr((char *)msg,"mousepo:"))  // mouse click upper WF
        {
            long long v1 = atol((char *)msg+8);   // full qrg from browser
            long long v2 = v1 - LNB_LO;
            long long v = v2 - (long long)TUNED_FREQUENCY;        // we need the offset only
            freqval = (int)v;
            setfreq = 1;
        }
        if(strstr((char *)msg,"mousewh:"))
        {
            freqval = atoi((char *)msg+8);
            setfreq = 2;
        }
        if(strstr((char *)msg,"bandsel:"))
        {
            freqval = atoi((char *)msg+8);
            setfreq = 3;
        }
        if(strstr((char *)msg,"ssbmode:"))
        {
            freqval = atoi((char *)msg+8);
            setfreq = 4;
        }
        if(strstr((char *)msg,"filterw:"))
        {
            freqval = atoi((char *)msg+8);
            setfreq = 5;
        }
        if(strstr((char *)msg,"tunerfr:"))
        {
            freqval = atoi((char *)msg+8);
            setfreq = 6;
        }
        if(strstr((char *)msg,"autosyn:"))
        {
            freqval = atoi((char *)msg+8);
            setfreq = 7;
        }
        if(strstr((char *)msg,"tunervl:"))
        {
            // Browser sends a new tuner frequency
            freqval = atoi((char *)msg+8);
            setfreq = 8;
        }
        if(strstr((char *)msg,"mouselo:"))  // mouse click lower WF
        {
            freqval = atoi((char *)msg+8);
            setfreq = 9;
        }
        if(strstr((char *)msg,"catonof:"))
        {
            freqval = atoi((char *)msg+8);
            setfreq = 10;
        }
        if(strstr((char *)msg,"catsetq:") && !memcmp(cli,"192.168",7))
        {
            // allow only local stations to activate CAT control
            freqval = atoi((char *)msg+8);
            setfreq = 11;
        }
        #ifdef WIDEBAND
        if(strstr((char *)msg,"datvqrg:"))
        {
            #if MINITIOUNER_LOCAL == 1
                if(!remoteaccess) 
                    setMinitiouner((char *)msg+8);
                else
                    printf("remote access to minitiouner blocked\n");
            #else
                setMinitiouner((char *)msg+8);
            #endif
        }
        #endif
        
        free(cli);
    }
}

void get_ownIP()
{
    struct ifaddrs *ifaddr, *ifa;
    int s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("getifaddrs");
        return;
    }


    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        s=getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);

        if(ifa->ifa_addr->sa_family==AF_INET)
        {
            if (s != 0)
            {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                return;
            }
            printf("Interface : <%s>\n",ifa->ifa_name );
            printf("  Address : <%s>\n", host);
            char *hp = strchr(host,'.');
            if(hp)
            {
                *hp=0;
                if(!strstr(host,"127"))
                {
                    // found the first real IP address
                    strcpy(myIP, host);
                    printf("my IP starts with %s.\n",myIP);
                    return;
                }
            }
        }
    }

    freeifaddrs(ifaddr);
}
