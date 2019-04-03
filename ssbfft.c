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
* ssbfft.c ... 
* 
* this function gets samples with a rate of 600.000 kS/s (300kHz bandwidth)
* we need an FFT with a resolution of 10 Hz per bin
* so we do the FFT every 60.000 samples (10 times per second)
* 
* 
*/

#include <fftw3.h>
#include <math.h>
#include <string.h>
#include "fifo.h"
#include "wf_univ.h"
#include "playSDReshail2.h"
#include "setqrg.h"
#include "audio_bandpass.h"


void beaconLock(fftw_complex *pd, int len);

#define FSSB_SRATE          600000
#define FSSB_RESOLUTION     10
#define FSSB_FFT_LENGTH     (FSSB_SRATE / FSSB_RESOLUTION)  // = 60.000
#define FSSB_NUM_BINS       (FSSB_FFT_LENGTH / 2)           // = 30.000 (the last one is on nyquist freq, there for it should not be used)

fftw_complex *din = NULL;				// input data for  fft, output data from ifft
fftw_complex *cpout = NULL;	            // ouput data from fft, input data to ifft
fftw_complex *cpssb = NULL;             // converted cpout, used as input for the inverse fft
fftw_plan plan = NULL, iplan = NULL;
int din_idx = 0;
int rflock = 0;

void init_fssb()
{
    char fn[300];
	sprintf(fn, "fssb_wisom");	// wisdom file for each capture rate

	fftw_import_wisdom_from_filename(fn);
  
    din   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FSSB_FFT_LENGTH);
	cpout = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FSSB_FFT_LENGTH);
    cpssb = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * FSSB_FFT_LENGTH);

    plan = fftw_plan_dft_1d(FSSB_FFT_LENGTH, din, cpout, FFTW_FORWARD, FFTW_MEASURE);
	iplan = fftw_plan_dft_1d(FSSB_FFT_LENGTH, cpssb, din, FFTW_BACKWARD, FFTW_MEASURE);

    fftw_export_wisdom_to_filename(fn);
}

void fssb_sample_processing(short *xi, short *xq, int numSamples)
{
    // copy the samples into the FFT input buffer
    for(int i=0; i<numSamples; i++)
    {
        din[din_idx][0] = xi[i];
		din[din_idx][1] = xq[i];
        din_idx++;
        
        // check if the fft input buffer is filled
        if(din_idx >= FSSB_FFT_LENGTH)
        {
            din_idx = 0;
            
            // the buffer is full, now lets execute the fft
            fftw_execute(plan);
            
            // this fft has generated FSSB_NUM_BINS bins in cpout
            // for the waterfall we need only 3000 (WF_WIDTH) bins
            double wfsamp[WF_WIDTH];
            int idx = 0;
            double real,imag;
            int wfbins;

            beaconLock(cpout,idx);            
            
            /*for(int x=27480; x<27520; x++)
            {
                cpout[x][0] = 0;
                cpout[x][1] = 0;
            }*/
   
            // create waterfall line and send to the client through a web socket
            for(wfbins=0; wfbins<FSSB_NUM_BINS; wfbins+=10)
            {
                // calculate absolute value
                real = cpout[wfbins][0];
                imag = cpout[wfbins][1];
                wfsamp[idx] = sqrt((real * real) + (imag * imag));
                wfsamp[idx] /= 50;
                //printf("%.0f\n",wfsamp[idx]);
                idx++;
            }
            
            drawWF(WFID_BIG,wfsamp, WF_WIDTH, WF_WIDTH, 1, 0, FSSB_SRATE/2, FSSB_RESOLUTION*10, DISPLAYED_FREQUENCY_KHZ, "\0");
            
            // NULL the negative frequencies, this eliminates the mirror image
            // (in case of LSB demodulation use this negative image and null the positive)
            for (i = FSSB_NUM_BINS; i < FSSB_FFT_LENGTH; i++)
            {
                cpout[i][0] = 0;
                cpout[i][1] = 0;
            }
            
            // shift the required frequency bins to 0 Hz (baseband)
            // "foffset" has the listening frequency in Hz
            // the FFT resolution is 10 Hz, so we take 1/10
            idx = foffset/10;
            
            memset(cpssb,0,sizeof(fftw_complex) * FSSB_FFT_LENGTH);
            
            for (i = 0; i < (3600/FSSB_RESOLUTION); i++)
            {
                cpssb[i][0] = cpout[idx][0];
                cpssb[i][1] = cpout[idx][1];
                if(++idx >= FSSB_NUM_BINS) idx=0;
            }
            
            // with invers fft convert the spectrum back to samples
            fftw_execute(iplan);
            
            // and copy samples into the resulting buffer
            // scaling: the FFT/iFFT multipies the samples by N (capture rate, i.e. 48000)
            double ssbsamples[FSSB_FFT_LENGTH];
            for (i = 0; i < FSSB_FFT_LENGTH; i++)
            {
                ssbsamples[i] = din[i][0];	                 // use real part only
                if(hwtype == 1)
                {
                    // SDRplay
                    ssbsamples[i] /= 5000;
                }
                if(hwtype == 2)
                {
                    // RTL_SDR
                    ssbsamples[i] /= 40000;
                }
                
                if(ssbsamples[i] > 32767 || ssbsamples[i] < -32767)
                    printf("ssbsamples too loud: %.0f\n",ssbsamples[i]);

                // here we have the samples with rate of 600.000
                // we need 8.000 for the sound card
                // lets decimate by 75 (FSSB_SRATE / 8000)
                static int audio_cnt = 0;
                static short b16samples[AUDIO_RATE];
                static int audio_idx = 0;
                if(++audio_cnt >= 75)
                {
                    audio_cnt = 0;
                    
                    b16samples[audio_idx] = audio_filter((short)(ssbsamples[i]));
                    //b16samples[audio_idx] = (short)(ssbsamples[i]);
                    if(++audio_idx >= AUDIO_RATE)
                    {
                        audio_idx = 0;
                        // we have AUDIO_RATE (8000) samples, this is one second
                        #ifdef SOUNDLOCAL
                        write_pipe(FIFO_AUDIO, (unsigned char *)b16samples, AUDIO_RATE*2);
                        #else
                        // lets pipe it to the browser through the web socket
                        write_pipe(FIFO_AUDIOWEBSOCKET, (unsigned char *)b16samples, AUDIO_RATE*2);
                        #endif
                    }
                }
                else
                {
                }
            }
        }
    }
}

void beaconLock(fftw_complex *pd, int len)
{
    return; 
    
    // AUTO es'hail-2 tune, lock on PSK beacon
    #define MAXNUM  5
    static int lastmax[MAXNUM];
    static int lmidx = 0;
    #define ref  (100*(10489800-10489525))
    //#define ref  (100*(10489550-10489525))
    static int srclower = ref-5000;
    static int srcupper = ref+5000;
    double max = -9999;
    int imax = 0;
    static int pass = 0;
    
    for(int i=srclower; i<srcupper; i++)
    {
        double v = sqrt((pd[i][0] * pd[i][0]) + (pd[i][1] * pd[i][1]));
        if(v>9000000)
        printf("%d:%.0f ",i,v);
        if(v > max) 
        {
            max = v;
            imax = i;
        }
    }
    printf("\n\n");
    return;
    
    lastmax[lmidx] = imax;
    if(++lmidx >= MAXNUM) lmidx=0;

    int eval=0;
    for(int i=0; i<MAXNUM; i++)
    {
        eval += lastmax[i];
    }
    eval /= MAXNUM;
    
    
    if(pass > MAXNUM && (eval < (ref-30) || eval > (ref+30)))
    {
        if(rflock)
        {
            if((ref - eval) >= 1)
                newrf++;
            else
                newrf--;
        }
        else
        {
            newrf += (ref - eval);
        }
        
        printf("%d refpixel: %d max: %.0f %d eval: %d\n",pass,ref,max,imax,eval);
        
        setrfoffset = 1;
        
        //printf("eval: %d newRF: %.0f\n",eval,newrf);
        //rflock = 0;
    }
    else 
    {
        rflock = 1;
        // if locked, reduce search range
        // so that near stations do not disturb the search
        srclower = (ref-200);
        srcupper = (ref+200);
        pass++;
    }
}

