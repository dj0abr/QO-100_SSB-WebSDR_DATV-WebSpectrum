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
* 
* fifo.c ... thread save fifo
* 
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <rtl-sdr.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "qo100websdr.h"
#include "fifo.h"


struct timespec timeToWait;
pthread_mutex_t crit_sec[NUM_OF_PIPES];
pthread_mutex_t waitmutex[NUM_OF_PIPES];
pthread_cond_t	semaphore[NUM_OF_PIPES];
#define LOCK(pn)	pthread_mutex_lock(&(crit_sec[pn]))
#define UNLOCK(pn)	pthread_mutex_unlock(&(crit_sec[pn]))
#define WAKEUP(pn)	pthread_cond_signal(&(semaphore[pn]));
void WAIT(int pn)	
{
	struct timespec timeToWait;
	struct timeval now;
	gettimeofday(&now,NULL);
	timeToWait.tv_sec = now.tv_sec;
	timeToWait.tv_nsec = (now.tv_usec+1000UL*10)*1000UL;	// wait for 10ms
	pthread_mutex_lock(&(waitmutex[pn])); 
	pthread_cond_timedwait(&(semaphore[pn]),&(waitmutex[pn]),&timeToWait);
	pthread_mutex_unlock(&(waitmutex[pn]));
}

int wridx[NUM_OF_PIPES];
int rdidx[NUM_OF_PIPES];
unsigned char pipebuffer[NUM_OF_PIPES][BUFFER_LENGTH][BUFFER_ELEMENT_SIZE];

void initpipe()
{
	for (int i = 0; i < NUM_OF_PIPES; i++)
	{
		pthread_mutex_init(&(crit_sec[i]),NULL);
		pthread_cond_init(&(semaphore[i]),NULL);
		pthread_mutex_init(&(waitmutex[i]),NULL);
		rdidx[i] = wridx[i] = 0;
	}
}

void removepipe()
{
	for (int i = 0; i < NUM_OF_PIPES; i++)
	{
        pthread_mutex_destroy(&(crit_sec[i]));
        pthread_cond_destroy(&(semaphore[i]));
        pthread_mutex_destroy(&(waitmutex[i]));
    }
}

char write_pipe(int pipenum, unsigned char *data, int len)
{
	if (pipenum >= NUM_OF_PIPES)
	{
		printf("invalid write_pipe pipnum: %d\n", pipenum);
		return 0;
	}

	// prüfe ob die Daten in ein Fifo-Element passen
	if (len == 0 || len >= (BUFFER_ELEMENT_SIZE - 2)) 
    {
        printf("element too long: %d (buffer: %d)\n", len,BUFFER_ELEMENT_SIZE - 2);
        return 0;
    }

	LOCK(pipenum);

	int *pwridx = &(wridx[pipenum]);
	unsigned char *ppbuf = pipebuffer[pipenum][*pwridx];

	// prüfe ob im Fifo noch Platz ist
	if (((*pwridx + 1) % BUFFER_LENGTH) == rdidx[pipenum])
	{
        //printf("FIFO full: %d\n",pipenum);
		UNLOCK(pipenum);
		return 0;
	}

	// schreibe Daten in den Fifo
	*ppbuf = len & 0xff;
	*(ppbuf+1) = (len >> 8) & 0xff;
	memcpy(ppbuf + 2, data, len);

    // stelle Schreibzeiger weiter
	if (++*pwridx == BUFFER_LENGTH) *pwridx = 0;
	UNLOCK(pipenum);

	// wecke den read_pipe auf damit das Lesen sofort reagiert und nicht den Timeout/Sleep abwarten muss
	WAKEUP(pipenum);

	return 1;
}

int read_pipe(int pipenum, unsigned char *data, int maxlen)
{
	if (pipenum >= NUM_OF_PIPES)
	{
		printf("invalid read_pipe pipnum: %d\n", pipenum);
		return 0;
	}

	LOCK(pipenum);

	int *prdidx = &(rdidx[pipenum]);
	unsigned char *ppbuf = pipebuffer[pipenum][*prdidx];

	if (*prdidx == wridx[pipenum])
	{
		// Fifo ist leer
		UNLOCK(pipenum);
		return 0;
	}

	// lese Daten
	int len = *(ppbuf+1);
	len <<= 8;
	len += *ppbuf;
	if (len != 0 && len <= maxlen && len <= (BUFFER_ELEMENT_SIZE - 2))
	{
		memcpy(data, ppbuf + 2, len);
	}
	else
		printf("read wrong len:%d\n", len);

	// stelle Lesezeiger weiter
	if (++*prdidx == BUFFER_LENGTH) *prdidx = 0;
	UNLOCK(pipenum);

	return len;
}

int read_pipe_wait(int pipenum, unsigned char *data, int maxlen)
{
	if (pipenum >= NUM_OF_PIPES)
	{
		printf("ERROR: read_pipe_wait pipnum: %d but MAX is %d, recompile program\n", pipenum, NUM_OF_PIPES);
		return 0;
	}

	int len = 0;

	while (1)
	{
		len = read_pipe(pipenum, data, maxlen);
		if (len > 0) break;
		// Timeout 10ms, oder sofortiges Aufwachen wenn Daten in den Fifo geschrieben werden
		WAIT(pipenum);
	}
	return len;
}

int NumberOfElementsInPipe(int pipenum)
{
	if (pipenum >= NUM_OF_PIPES)
	{
		printf("NumberOfElementsInPipe pipnum: %d\n", pipenum);
		return 0;
	}

	int num = 0;

	LOCK(pipenum);
	// Anzahl der Elemente im FIFO
	num = (wridx[pipenum] + BUFFER_LENGTH - rdidx[pipenum]) % BUFFER_LENGTH;

	UNLOCK(pipenum);

	return num;
}
