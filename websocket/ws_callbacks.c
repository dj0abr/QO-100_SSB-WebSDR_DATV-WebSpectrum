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
#include "../setqrg.h"
#include "../fifo.h"
#include "../setup.h"

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
int access_blocked = 0;
static int f=1;
USERMSG tx_usermsg;

    if(f == 1)
    {
        // init own IP
        f=0;
        get_ownIP();
    }

	char *cli = ws_getaddress(fd);
    if(cli != NULL)
    {
        tx_usermsg.client = get_socket_idx(fd);
        tx_usermsg.command = 0;
        
        // check if IP is authorized to control the SDRplay
        if(memcmp(cli,myIP,strlen(myIP)) && !allowRemoteAccess)
        {
            access_blocked = 1;
            //printf("BLOCK some user messages: %s, from: %s/%d\n", msg, cli, fd);
        }
        else
            printf("user message: %s, from: %s/%d\n", msg, cli, fd);
        
        if(strstr((char *)msg,"mousepo:"))  // mouse click upper WF
        {
            long long v1 = atoll((char *)msg+8);   // full qrg from browser
            long long v2 = v1 - lnb_lo;
            long long v = v2 - (int64_t)tuned_frequency;        // we need the offset only
            //printf("%lld %lld %lld %lld %lld\n",v1,v2,v,(int64_t)lnb_lo,(int64_t)TUNED_FREQUENCY);
            tx_usermsg.command = 1;
            tx_usermsg.para = (int)v;
        }
        if(strstr((char *)msg,"mousewh:"))
        {
            tx_usermsg.command = 2;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"bandsel:"))
        {
            tx_usermsg.command = 3;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"ssbmode:"))
        {
            tx_usermsg.command = 4;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"tunerfr:"))
        {
            tx_usermsg.command = 6;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"tunervl:"))
        {
            tx_usermsg.command = 8;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"mouselo:"))  // mouse click lower WF
        {
            tx_usermsg.command = 9;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"catonof:") && !access_blocked)
        {
            tx_usermsg.command = 10;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"audioon:"))
        {
            tx_usermsg.command = 12;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        if(strstr((char *)msg,"getconf:"))
        {
            configrequest = 1;
        }
        
        if(strstr((char *)msg,"cfgdata:") && !access_blocked)
        {
            getConfigfromBrowser((char *)msg+8);
        }
        
        if(strstr((char *)msg,"seticom:") && !access_blocked)
        {
            tx_usermsg.command = 13;
            long long f = atoll((char *)msg+8); // requested frequency 
            long long MHz = (f / 1000000)*1000000;
            f -= MHz;
            printf("got kHz: %d",(int)f);
            tx_usermsg.para = (int)f;
        }
        
        if(strstr((char *)msg,"seticlo:") && !access_blocked)
        {
            tx_usermsg.command = 14;
            tx_usermsg.para = atoi((char *)msg+8);
        }
        
        #ifdef WIDEBAND
        if(strstr((char *)msg,"datvqrg:"))
        {
            if(minitiouner_local == 1)
            {
                if(!access_blocked) 
                {
                    //setMinitiouner((char *)msg+8);
                    tx_usermsg.command = 15;
                    strcpy(tx_usermsg.spara,((char *)msg+8));
                }
                else
                    printf("remote access to minitiouner blocked\n");
            }
            else
            {
                //setMinitiouner((char *)msg+8);
                tx_usermsg.command = 15;
                strcpy(tx_usermsg.spara,((char *)msg+8));
            }
        }
        #endif
        
        if(tx_usermsg.command != 0)
        {
            write_pipe(FIFO_USERCMD, (unsigned char *)(&tx_usermsg), sizeof(USERMSG));
        }
        
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
