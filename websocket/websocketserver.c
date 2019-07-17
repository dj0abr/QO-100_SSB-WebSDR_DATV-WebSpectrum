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

WS_SOCK actsock[MAX_CLIENTS];

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
        printf("wf_drawline: proc NOT started\n\r");
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
	ws_socket(&evs, WEBSOCK_PORT);
    
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
pthread_mutex_t crit_sec;
#define LOCK	pthread_mutex_lock(&crit_sec)
#define UNLOCK	pthread_mutex_unlock(&crit_sec)

void ws_send(unsigned char *pwfdata, int idx, int wf_id)
{
    // insert the new message into the buffer of each active client
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket != -1)
        {
            if(wf_id == 0 && actsock[i].send0 == 0)
            {
                memcpy(actsock[i].msg0, pwfdata, idx);
                actsock[i].msglen0 = idx;
                actsock[i].send0 = 1;
            }
            
            if(wf_id == 1 && actsock[i].send1 == 0)
            {
                memcpy(actsock[i].msg1, pwfdata, idx);
                actsock[i].msglen1 = idx;
                actsock[i].send1 = 1;
            }
        }
    }
    
    // if we have audio samples in the fifo, then copy it to each clients audio buffer
    unsigned char samples[AUDIO_RATE*2];
    int len = read_pipe(FIFO_AUDIOWEBSOCKET, samples, AUDIO_RATE*2);
    if(len)
    {
        for(int i=0; i<MAX_CLIENTS; i++)
        {
            if(actsock[i].socket != -1)
            {
                memcpy(actsock[i].samples,samples,AUDIO_RATE*2);
                actsock[i].sendaudio = 1;
            }
        }
    }
    
    UNLOCK;
}

unsigned char *ws_build_txframe(int i, int *plength)
{
    LOCK;
    
    if(actsock[i].sendaudio == 1)
    {
        int idx = 0;
        int len = AUDIO_RATE*2;
        unsigned char *txdata = malloc(len+3);
        if(txdata == NULL)
            return NULL;
        //printf("audio:%d\n",len);
        txdata[idx++] = 2;    // type of audio samples
        txdata[idx++] = len >> 8;
        txdata[idx++] = len & 0xff;
        memcpy(txdata+idx,actsock[i].samples,len);
        idx += len;

        *plength = idx;
        actsock[i].sendaudio = 0;
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
        return NULL;
    
    // copy data into TX buffer and set the type byte
    int idx = 0;
    if(actsock[i].send0 == 1) 
    {
        txdata[idx++] = 0;    // type of big WF
        txdata[idx++] = actsock[i].msglen0 >> 8;
        txdata[idx++] = actsock[i].msglen0 & 0xff;
        memcpy(txdata+idx,actsock[i].msg0,actsock[i].msglen0);
        // first WF byte is used as RF-lock indicator
        txdata[idx] = rflock;
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
void insert_socket(int fd)
{
    LOCK;
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(actsock[i].socket == -1)
        {
            actsock[i].socket = fd;
            actsock[i].send0 = 0;
            actsock[i].send1 = 0;
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
            actsock[i].socket = -1;
    }
    UNLOCK;
}
