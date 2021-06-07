/*
* Web based SDR Client for SDRplay
* =============================================================
* Author: DJ0ABR
*
*   (c) DJ0ABR
*   www.dj0abr.de
* 
* most websocket related function are written by
* Davidson Francis <davidsondfgl@gmail.com>
* under the same license (available at github)
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
* websocketserver.c ... sends data to a web browser
* 
* if a web browser is logged into this WebSocketServer then
* we send the pixel data (of one line) and header data to this browser.
* Its the job of the browser to draw the waterfall picture.
* 
* This WebSocketServer is a multi threaded implementation and
* opens a new thread for each client
* 
* ! THIS implementation of a WebSocketServer DOES NOT require any
* modifications of the (Apache) Webserver. It has nothing to do with
* the webserver because it handles all Websocket tasks by itself !
* 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "../qo100websdr.h"
#include "websocketserver.h"
#include "../fifo.h"
#include "../ssbfft.h"
#include "../setup.h"

#define MAXIPLEN 16

WS_SOCK actsock[MAX_CLIENTS];
char clilist[MAX_CLIENTS][MAXIPLEN];

void *wsproc(void *pdata);

// initialise the WebSocketServer
// run the WebSocketServer in a new thread
void ws_init()
{
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        actsock[i].socket = -1;
        actsock[i].send0 = 0;
        actsock[i].send1 = 0;
    }
            
    pthread_t ws_pid = 0;
    
    int ret = pthread_create(&ws_pid,NULL,wsproc, NULL);
    if(ret)
    {
        printf("wsproc: proc NOT started\n\r");
    }
}

void *wsproc(void *pdata)
{
    // this thread must terminate itself because
    // the parent does not want to wait
    pthread_detach(pthread_self());

    struct ws_events evs;
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;
    evs.onwork    = &onwork;
	ws_socket(&evs, websock_port);
    
    // never comes here because ws_socket runs in an infinite loop
    pthread_exit(NULL); // self terminate this thread
}

/*
 * save the data in an array and set a flag.
 * the transmission to the web browser is done by the threads
 * handling all logged-in web browsers.
 * 
 * this function is thread safe by a LOCK
 */
pthread_mutex_t ws_crit_sec;
#define LOCK	pthread_mutex_lock(&ws_crit_sec)
#define UNLOCK	pthread_mutex_unlock(&ws_crit_sec)

void ws_send(unsigned char *pwfdata, int idx, int wf_id, int client)
{
    // insert the new message into the buffer of each active client
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket != -1 && i == client)
        {
            // insert big WF data
            if(wf_id == 0 && actsock[i].send0 == 0)
            {
                memcpy(actsock[i].msg0, pwfdata, idx);
                actsock[i].msglen0 = idx;
                actsock[i].send0 = 1;
            }
            
            // insert small WF data
            if(wf_id == 1 && actsock[i].send1 == 0)
            {
                memcpy(actsock[i].msg1, pwfdata, idx);
                actsock[i].msglen1 = idx;
                actsock[i].send1 = 1;
            }
        }
    }
    
    UNLOCK;
}

void ws_send_audio()
{
unsigned char samples[AUDIO_RATE*2];
int len;

    LOCK;
    
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        // insert audio samples
        len = read_pipe(FIFO_AUDIOWEBSOCKET + i, samples, AUDIO_RATE*2);
        if(len)
        {
            memcpy(actsock[i].samples,samples,AUDIO_RATE*2);
            actsock[i].sendaudio = 1;
        }
    }
    
    UNLOCK;
}

void ws_send_config(unsigned char *cfgdata, int len)
{
    LOCK;
    
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        // send to all connected clients
        if(actsock[i].socket != -1)
        {
            memcpy(actsock[i].msg3, cfgdata, len);
            actsock[i].msglen3 = len;
            actsock[i].send3 = 1;
        }
    }
    
    UNLOCK;
}

void ws_send_users()
{
    // no LOCK needed, this is already done in the calling function
    
    // collect active users
    char cfgdata[MAX_CLIENTS * MAXIPLEN + 1];
    *cfgdata = 0;
    int len = 0;
    printf("currently active users:\n");
    for(int ips=0; ips<MAX_CLIENTS; ips++)
    {
        if(actsock[ips].socket != -1)
        {
            printf("IP: %s\n",clilist[ips]);
            sprintf(cfgdata+strlen(cfgdata),"%16.16s",clilist[ips]);
            len += 16;
        }
    }
 
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        // send to all connected clients
        if(actsock[i].socket != -1)
        {
            memcpy(actsock[i].msg4, cfgdata, len);
            actsock[i].msglen4 = len;
            actsock[i].send4 = 1;
        }
    }
}

unsigned char *ws_build_txframe(int i, int *plength)
{
    LOCK;
    
    if(actsock[i].sendaudio == 1)
    {
        // send audio samples
        int idx = 0;
        int len = AUDIO_RATE*2;
        unsigned char *txdata = malloc(len+3);
        if(txdata == NULL)
        {
            UNLOCK;
            return NULL;
        }
        txdata[idx++] = 2;    // type: audio samples
        txdata[idx++] = len >> 8;
        txdata[idx++] = len & 0xff;
        memcpy(txdata+idx,actsock[i].samples,len);
        idx += len;

        *plength = idx;
        actsock[i].sendaudio = 0;
        UNLOCK;
        return txdata;
    }
    
    if(actsock[i].send3 == 1)
    {
        // send config data
        int idx = 0;
        int len = actsock[i].msglen3;
        unsigned char *txdata = malloc(len+3);
        if(txdata == NULL)
        {
            UNLOCK;
            return NULL;
        }
        txdata[idx++] = 3;    // type: config data
        txdata[idx++] = len >> 8;
        txdata[idx++] = len & 0xff;
        memcpy(txdata+idx,actsock[i].msg3,len);
        idx += len;
        actsock[i].send3 = 0;
        *plength = idx;
        UNLOCK;
        return txdata;
    }

    if(actsock[i].send4 == 1)
    {
        // send active users
        int idx = 0;
        int len = actsock[i].msglen4;
        unsigned char *txdata = malloc(len+3);
        if(txdata == NULL)
        {
            UNLOCK;
            return NULL;
        }
        txdata[idx++] = 4;    // type: user data
        txdata[idx++] = len >> 8;
        txdata[idx++] = len & 0xff;
        memcpy(txdata+idx,actsock[i].msg4,len);
        idx += len;
        actsock[i].send4 = 0;
        *plength = idx;
        UNLOCK;
        return txdata;
    }

    // calculate total length
    // add 3 for each element for the first byte which is the element type followed by the length
    int geslen = 0;
    if(actsock[i].send0 == 1) geslen += (actsock[i].msglen0 + 3);
    if(actsock[i].send1 == 1) geslen += (actsock[i].msglen1 + 3);
    
    if(geslen < 10) 
    {
        UNLOCK;
        return NULL;
    }
    
    // assign TX buffer
    unsigned char *txdata = malloc(geslen);
    if(txdata == NULL)
    {
        UNLOCK;
        return NULL;
    }
    
    // copy data into TX buffer and set the type byte
    int idx = 0;
    if(actsock[i].send0 == 1) 
    {
        txdata[idx++] = 0;    // type of big WF
        txdata[idx++] = actsock[i].msglen0 >> 8;
        txdata[idx++] = actsock[i].msglen0 & 0xff;
        memcpy(txdata+idx,actsock[i].msg0,actsock[i].msglen0);
        // first WF byte is used as RF-lock indicator
        #ifndef EXTUDP
        txdata[idx] = rflock;
        #else
        txdata[idx] = 1;
        #endif
        idx += actsock[i].msglen0;
    }
    
    if(actsock[i].send1 == 1) 
    {
        txdata[idx++] = 1;    // type of small WF
        txdata[idx++] = actsock[i].msglen1 >> 8;
        txdata[idx++] = actsock[i].msglen1 & 0xff;
        memcpy(txdata+idx,actsock[i].msg1,actsock[i].msglen1);
        idx += actsock[i].msglen1;
    }

    *plength = idx;
    
    UNLOCK;
    return txdata;
}


// insert a socket into the socket-list
void insert_socket(int fd, char *cli)
{
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket == -1)
        {
            actsock[i].socket = fd;
            actsock[i].send0 = 0;
            actsock[i].send1 = 0;
            strncpy(clilist[i],cli,MAXIPLEN);
            clilist[i][MAXIPLEN-1] = 0;
            printf("accepted client %d %d\n",i,fd);
            
            // send active users
            ws_send_users();
            
            UNLOCK;
            
            return;
        }
    }
    UNLOCK;
    printf("all sockets in use !!!\n");
}

// remove a socket from the socket-list
void remove_socket(int fd)
{
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket == fd)
        {
            actsock[i].socket = -1;
            clilist[i][0] = 0;
        }
    }
    
    // send active users
    ws_send_users();
            
    UNLOCK;
}

// test if a cli (IP) is already logged in
// 1=yes, 0=no
int test_socket(char *cli)
{
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        // if active: only one connection per IP allowed
        /*if(!strcmp(cli,clilist[i])) 
        {
            printf("%s is already logged in, ignore\n",cli);
            UNLOCK;
            return 1;
        }*/
    }
    UNLOCK;
    return 0;
}

int get_socket_idx(int fd)
{
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket == fd)
        {
            UNLOCK;
            return i;
        }
    }
    UNLOCK;
    return -1;
}

int get_useranz()
{
int us=0;

    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket != -1)
            us++;
    }
    UNLOCK;
    return us;
}

// get IP corresponding to a socket
char *getSocketIP(int fd)
{
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket == fd)
        {
            UNLOCK;
            return clilist[i];
        }
    }
    UNLOCK;
    return NULL;
}

// check if the IP of an index is local
int isLocal(int idx)
{
    if(!memcmp(clilist[idx],myIP,strlen(myIP)))
        return 1;
    
    return 0;
}


