/* Web based SDR Client for SDRplay
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
* ssbdemod.c ... 
* 
* demodulates an SSB signal, uses a separate thread for each client
* demodulation is done by an inverse FFT
* 
*/

#include <fftw3.h>
#include <pthread.h>
#include <time.h>
#include "qo100websdr.h"
#include "ssbdemod.h"
#include "websocketserver.h"
#include "setqrg.h"
#include "audio_bandpass.h"
#include "fifo.h"

fftw_complex *cin[MAX_CLIENTS];
fftw_complex *cout[MAX_CLIENTS];
fftw_plan iplan[MAX_CLIENTS];
int16_t b16samples[MAX_CLIENTS][AUDIO_RATE];
int b16idx[MAX_CLIENTS];

/*
 * handling all clients in one thread takes less CPU time
 * than using a separate thread for each client
 */
#define CLIENT_HANDLING 1   // 0=all in one thread, 1=one thread per client

#if CLIENT_HANDLING == 1
pthread_mutex_t ssb_crit_sec[MAX_CLIENTS];
#define SSB_LOCK(cli)	pthread_mutex_lock(&(ssb_crit_sec[cli]))
#define SSB_UNLOCK(cli)	pthread_mutex_unlock(&(ssb_crit_sec[cli]))

typedef struct {
    int client;
    int running;
    fftw_complex *cpout;
} SSBPARAM;

SSBPARAM ssbp[MAX_CLIENTS];
#endif

void init_ssbdemod()
{
    for(int cli=0; cli<MAX_CLIENTS; cli++)
    {
        cin[cli]   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * NB_FFT_LENGTH);
        cout[cli] = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * NB_FFT_LENGTH);
        iplan[cli] = fftw_plan_dft_1d(NB_FFT_LENGTH, cin[cli], cout[cli], FFTW_BACKWARD, FFTW_MEASURE);
        
        b16idx[cli] = 0;
        #if CLIENT_HANDLING == 1
        ssbp[cli].running = 0;
        #endif
    }
}

void ssbdemod(fftw_complex *cpout)
{
#if CLIENT_HANDLING == 0
    pthread_t client_thread;

    if(pthread_create(&client_thread, NULL, ssbdemod_thread, (void*)(intptr_t)cpout) < 0)
        perror("Could not create the client thread!");

    pthread_detach(client_thread); // automatically release all ressources as soon as the thread is done
#else
    
    for(int cli=0; cli<MAX_CLIENTS; cli++)
    {
        if(actsock[cli].socket != -1)
        {
            ssbp[cli].client = cli;
            ssbp[cli].cpout = cpout;
            
            while(1)
            {
                SSB_LOCK(cli);
                if(ssbp[cli].running == 0)
                {
                    SSB_UNLOCK(cli);
                    break;
                }
                SSB_UNLOCK(cli);
                usleep(100);
            }
            
            pthread_t client_thread;

            if(pthread_create(&client_thread, NULL, ssbdemod_thread, (void*)(intptr_t)cli) < 0)
                perror("Could not create the client thread!");

            pthread_detach(client_thread); // automatically release all ressources as soon as the thread is done
        }
    }
#endif    
}

void *ssbdemod_thread(void *param)
{
#if CLIENT_HANDLING == 1
    int client = (int)(intptr_t)param;
    fftw_complex *cpout = ssbp[client].cpout;
#else
    fftw_complex *cpout = (fftw_complex *)param;
    for(int client=0; client<MAX_CLIENTS; client++)
    {
        if(actsock[client].socket == -1) continue;
#endif
        
        // SSB Demodulation
        // we got 90.000 values, 10 Hz/value
        // shift the demodulated signal from the IF into the baseband (0 Hz)
        // fmin and fmax are the minimum and maximum audio frequencies of interest
        int fmin = 0;
        int fmax = 500;                     // 3,6 kHz max BW
        int ifqrg = foffset[client] / 10;   // offset choosen by the user
        for (int i = fmin; i < fmax; i++)
        {
            cin[client][i][0] = cpout[i+ifqrg][0];
            cin[client][i][1] = cpout[i+ifqrg][1];
        }
        
        // NULL the negative frequencies, this eliminates the mirror image
        // (in case of LSB demodulation use this negative image and null the positive)
        for (int i = (NB_FFT_LENGTH / 2); i < NB_FFT_LENGTH; i++)
        {
            cin[client][i][0] = 0;
            cin[client][i][1] = 0;
        }

        // with invers fft convert the spectrum back to samples
        fftw_execute(iplan[client]);

        // and copy samples into the resulting buffer
        // we have NB_FFT_LENGTH usbsamples for 1/10s
        // fill a buffer for 1s
        // scale down to AUDIORATE
        #define maxcode  (32767.0 / 2.0) // abs max = 32767
        static double max[MAX_CLIENTS];
        for(int i=0; i<NB_FFT_LENGTH; i+=(NB_FFT_LENGTH/(AUDIO_RATE/10)))
        {
            // scale to max. 16 bit
            double v = cout[client][i][0];
            if(v > 0 && v > max[client]) max[client] = v;
            if(v < 0 && -v > max[client]) max[client] = -v;
            if(max[client] <= 0) max[client] = 1;
            
            // filter and copy sample to audio sample buffer
            b16samples[client][(b16idx[client])++] = audio_filter((int16_t)(v*maxcode/max[client]),client);

            // reduce scaling slowly
            if(max[client] > 1) max[client]-=100;
        }
        if(b16idx[client] == AUDIO_RATE) 
        {
            // audio samples for 1s available, send to browser
            b16idx[client] = 0;
            write_pipe(FIFO_AUDIOWEBSOCKET + client, (unsigned char *)b16samples[client], AUDIO_RATE*2);
        }
#if CLIENT_HANDLING == 0
    }
#else
    SSB_LOCK(client);
    ssbp[client].running = 0;
    SSB_UNLOCK(client);
#endif
    return NULL;
}
