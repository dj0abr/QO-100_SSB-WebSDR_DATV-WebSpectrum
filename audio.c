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
* audio.c ... 
* 1) play the demodulated samples to the sound card
* 2) write the samples into a fifo (which is read by
* the WebSocket Server and set to the browsers)
* 
*/

#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sndfile.h>
#include <time.h>
#include <sys/time.h>
#include "fifo.h"
#include "audio.h"
#include "qo100websdr.h"

void *audioproc(void *pdata);
void expand_stereo(short *samples, int len);

snd_pcm_t *playback_handle=NULL;
char *snddevice = {"pulse"};

void init_soundprocessing()
{
    pthread_t audio_pid = 0;
    
    int ret = pthread_create(&audio_pid,NULL,audioproc, NULL);
    if(ret)
    {
        printf("init_soundprocessing: proc NOT started\n\r");
    }
}


void *audioproc(void *pdata)
{
short samples[AUDIO_RATE*2];

    // this thread must terminate itself because
    // the parent does not want to wait
    pthread_detach(pthread_self()); 
    
    while(1)
    {
        int len = read_pipe_wait(FIFO_AUDIO, (unsigned char *)samples, AUDIO_RATE*2);
        expand_stereo(samples, len/2);
        play_samples(samples, len/2);
    }
    
    pthread_exit(NULL); // self terminate this thread
}

// expand the mono stream to expand_stereo
// by simple doubling the samples
void expand_stereo(short *samples, int len)
{
    for(int i=(len-1); i>=0; i--)
    {
        samples[i*2] = samples[i];
        samples[i*2+1] = samples[i];
    }
}

int play_samples(short *samples, int len)
{   
int err;

    if(playback_handle == NULL)
    {
        int ret = init_soundcard(snddevice);
        if(ret == 1)
        {
            for(int i=0; i<4; i++)
            {
                if ((err = snd_pcm_writei(playback_handle, samples, len)) != len) 
                {
                    printf("%d: write to audio interface failed len:%d ret:%d (%s)\n", i, len, err, snd_strerror(err));
                    playback_handle = NULL;
                    return 0;
                }
            }
            return 1;
        }
        return 0;
    }
    else
    {
        if ((err = snd_pcm_writei(playback_handle, samples, len)) != len) 
        {
            printf("write to audio interface failed len:%d ret:%d (%s)\n", len, err, snd_strerror(err));
            playback_handle = NULL;
            return 0;
        }
        return 1;
    }
}

// initialize the sound device for playback
// usually I use "pulse" as sndcard because then
// we can use pavucontrol to control and route the audio
int init_soundcard(char *sndcard)
{
	int err = 0;
	int channels = 2;
	snd_pcm_hw_params_t *hw_params;
    
    int rate = AUDIO_RATE;
    
    printf("open %s\n\r",sndcard);

    if ((err = snd_pcm_open(&playback_handle, sndcard, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        printf("please disable playback. cannot open audio device %s (%s)\n", sndcard, snd_strerror(err));
        
    }

    else if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        printf("please disable playback.cannot allocate hardware parameter structure (%s)\n", snd_strerror(err));
        
    }

    else if ((err = snd_pcm_hw_params_any(playback_handle, hw_params)) < 0) {
        printf("please disable playback. cannot initialize hardware parameter structure (%s)\n", snd_strerror(err));
        
    }

    else if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("please disable playback.cannot set access type (%s)\n", snd_strerror(err));
        
    }

    else if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
        printf("please disable playback.cannot set sample format (%s)\n", snd_strerror(err));
        
    }

    //else if ((err = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0)) < 0) {
    else if ((err = snd_pcm_hw_params_set_rate(playback_handle, hw_params, rate, 0)) < 0) 
    {
        printf("please disable playback. cannot set sample rate (%s)\n", snd_strerror(err));
        exit(0);
    }

    else if ((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params, channels)) < 0) {
        printf("please disable playback. cannot set channel count (%s)\n", snd_strerror(err));
        
    }

    else if ((err = snd_pcm_hw_params(playback_handle, hw_params)) < 0) {
        printf("please disable playback. cannot set parameters (%s)\n", snd_strerror(err));
        
    }
    else
    {
        snd_pcm_hw_params_free(hw_params);

        if ((err = snd_pcm_prepare(playback_handle)) < 0) {
            printf("please disable playback. cannot prepare audio interface for use (%s)\n", snd_strerror(err));
            
        }
    }
    if(err < 0)
    {
        if (playback_handle) snd_pcm_close(playback_handle);
        playback_handle = NULL;
        printf("error, audio not opened\n");
        return 0;
    }
    return 1;
}
