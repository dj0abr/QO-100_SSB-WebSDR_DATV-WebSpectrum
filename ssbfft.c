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
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "fifo.h"
#include "wf_univ.h"
#include "qo100websdr.h"
#include "setqrg.h"
#include "audio_bandpass.h"
#include "downmixer.h"
#include "hilbert90.h"
#include "antialiasing.h"
#include "timing.h"

void bcnLock(unsigned short *vals, int len);

fftw_complex *din = NULL;				// input data for  fft, output data from ifft
fftw_complex *cpout = NULL;	            // ouput data from fft, input data to ifft
fftw_complex *cpssb = NULL;             // converted cpout, used as input for the inverse fft
fftw_plan plan = NULL, iplan = NULL;
int din_idx = 0;
int rflock = 0;

#ifndef WIDEBAND

void init_fssb()
{
    char fn[300];
	sprintf(fn, "fssb_wisdom");	// wisdom file for each capture rate

	fftw_import_wisdom_from_filename(fn);
  
    din   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * NB_FFT_LENGTH);
	cpout = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * NB_FFT_LENGTH);
    cpssb = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * NB_FFT_LENGTH);

    plan = fftw_plan_dft_1d(NB_FFT_LENGTH, din, cpout, FFTW_FORWARD, FFTW_MEASURE);
	iplan = fftw_plan_dft_1d(NB_FFT_LENGTH, cpssb, din, FFTW_BACKWARD, FFTW_MEASURE);

    fftw_export_wisdom_to_filename(fn);
}

// gain correction
// opper big waterfall
int ssb_gaincorr_rtl = 2000; // if wfsamp[] overflows 16 bit then make this value higher
int small_gaincorr_rtl = 2000;

// lower waterfall
int ssb_gaincorr_sdrplay = 1000;
int small_gaincorr_sdrplay = 1000;
int ex=0;
void fssb_sample_processing(short *xi, short *xq, int numSamples)
{
    // copy the samples into the FFT input buffer
    for(int i=0; i<numSamples; i++)
    {
        din[din_idx][0] = xi[i];
		din[din_idx][1] = xq[i];
        din_idx++;
        
        // check if the fft input buffer is filled
        if(din_idx >= NB_FFT_LENGTH)
        {
            din_idx = 0;
            
            // the buffer is full, now lets execute the fft
            fftw_execute(plan);
            
            // this fft has generated FSSB_NUM_BINS bins in cpout
            #define DATASIZE ((NB_FFT_LENGTH/2)/NB_OVERSAMPLING)    // (180.000/2)/10 = 9000 final values
            unsigned short wfsamp[DATASIZE];
            int idx = 0;
            double real,imag;
            int wfbins;
            
            // we do not use the possible lower/upper half of the FFT because the Nyquist frequency
            // is in the middle and makes an awful line in the middle of the waterfall
            // therefore we use the double sampling frequency and display to lower half only

            for(wfbins=0; wfbins<(NB_FFT_LENGTH/2); wfbins+=NB_OVERSAMPLING)
            {
                if(idx >= DATASIZE) break; // all wf pixels are filled
                
                double maxv = -99999;
                for(int bin10=0; bin10<NB_OVERSAMPLING; bin10++)
                {
                    real = cpout[wfbins+bin10][0];
                    imag = cpout[wfbins+bin10][1];
                    double v = sqrt((real * real) + (imag * imag));
                    //beaconLock(v,wfbins+bin10);
                    if(v > maxv) maxv = v;
                }

                // level correction
                if(hwtype == 2)
                    maxv /= ssb_gaincorr_rtl;
                 else
                    maxv /= ssb_gaincorr_sdrplay;
                 
                if(maxv > 65535) 
                {
                    //printf("maxv overflow, rise gaincorr\n");
                    maxv = 65535;
                }
                wfsamp[idx] = (unsigned short)maxv;

                idx++;
            }
            
            // here war have wfsamp filled with DATASIZE values
            
            // make Beacon Lock
            bcnLock(wfsamp,DATASIZE);
            
            // showing the spectrum from 0-900.000 Hz with a resolution of 10 Hz
            
            unsigned int realrf = TUNED_FREQUENCY - newrf;
            
			drawWF( WFID_BIG,                   // Waterfall ID
                    wfsamp,                     // FFT output data
                    realrf,            			// frequency of the SDR 
                    WF_RANGE_HZ,                // total width of the fft data in Hz (900.000)
                    NB_HZ_PER_PIXEL,            // Hz/pixel (100)
                    DISPLAYED_FREQUENCY_KHZ);   // frequency of the left margin of the waterfall
            
            // for the SMALL waterfall we need 1500 (WF_WIDTH) bins
            // in a range of 15.000 Hz, so every single bin (one bin is 10 Hz)
            // starting at the current RX frequency - 15kHz/2 (so the RX qrg is in the middle)
            // foffset is the RX qrg in Hz
            // so we need the bins from (foffset/10) - 750 to (foffset/10) + 750
            int start = ((foffset)/10) - 750;
            int end = ((foffset)/10) + 750;

			if(start < 0) start = 0;
			if(end >= NB_FFT_LENGTH) end = NB_FFT_LENGTH-1;
            
            unsigned short wfsampsmall[WF_WIDTH];
            idx = 0;

            for(wfbins=start; wfbins<end; wfbins++)
            {
                real = cpout[wfbins][0];
                imag = cpout[wfbins][1];
                double dm = sqrt((real * real) + (imag * imag));
                
                // correct level
                
                if(hwtype == 2)
                    dm /= small_gaincorr_rtl;
                 else
                    dm /= small_gaincorr_sdrplay;
				if(dm > 65535) 
                {
                    //printf("maxv overflow, rise small_gaincorr dm:%.0f\n",dm);
                    dm = 65535;
                }
				wfsampsmall[idx] = (unsigned short)dm;
                
                idx++;
            }

			drawWF( WFID_SMALL,                 // Waterfall ID
                    wfsampsmall,                // FFT output data
                    realrf,            			// frequency of the SDR 
                    15000,               		// total width of the fft data in Hz (in this case 8.000.000)
                    10,			                // Hz/pixel
                    DISPLAYED_FREQUENCY_KHZ + (foffset)/1000);   // frequency of the left margin of the waterfall
        }
        
        // SSB Decoding and Audio Output
        // shift the needed frequency to baseband
        downmixer_process(xi+i,xq+i);
        
        // here we have the samples with rate of 1.800.000
        // we need 7.500 for the sound card
        // lets decimate by 240 (2*WF_RANGE_HZ / 8000)
        static int audio_cnt = 0;
        static short b16samples[AUDIO_RATE];
        static int audio_idx = 0;
        if(++audio_cnt >= 225)
        {
            audio_cnt = 0;
            
            xi[i] = fir_filter_i_ssb(xi[i]);
            xq[i] = fir_filter_q_ssb(xq[i]);
    
            // USB demodulation
            double dsamp = BandPassm45deg(xi[i]) - BandPass45deg(xq[i]);
            
            if(dsamp > 32767 || dsamp < -32767)
                printf("Hilbert Output too loud: %.0f\n",dsamp);
            
            b16samples[audio_idx] = audio_filter((short)dsamp);
            
            if(++audio_idx >= AUDIO_RATE)
            {
                audio_idx = 0;
                // we have AUDIO_RATE (8000) samples, this is one second
                #ifdef SOUNDLOCAL
                    // play it to the local soundcard
                    write_pipe(FIFO_AUDIO, (unsigned char *)b16samples, AUDIO_RATE*2);
                #else
                    // lets pipe it to the browser through the web socket
                    write_pipe(FIFO_AUDIOWEBSOCKET, (unsigned char *)b16samples, AUDIO_RATE*2);
                #endif
            }
        }
        else
        {
            fir_filter_i_ssb_inc(xi[i]);
            fir_filter_q_ssb_inc(xq[i]);
        }
    }
}

// CW Beacon
// calculate the position (offset in the FFT output values) of the CW beacon
// NB_FFT_LENGTH/2 goes over a range of WF_RANGE_HZ Hz starting at DISPLAYED_FREQUENCY_KHZ
// with a resolution of 10Hz per FFT value
#define BEACON_OFFSET   (((long long)CW_BEACON - (long long)DISPLAYED_FREQUENCY_KHZ * (long long)1000L) / ((long long)NB_HZ_PER_PIXEL))
#define BEACON_LOCKRANGE        (long long)200     // check beacon QRG +- lockrange (1=100Hz)

// PSK Beacon
// has a width of +-600Hz
#define PSK_BEACON_OFFSET   (((long long)PSK_BEACON - (long long)DISPLAYED_FREQUENCY_KHZ * (long long)1000L) / ((long long)NB_HZ_PER_PIXEL))
#define PSK_BEACON_LOCKRANGE        (long long)6     // check beacon QRG +- lockrange (1=100Hz)
#define PSK_CW_OFFSET (PSK_BEACON_OFFSET - BEACON_OFFSET)

void bcnLock(unsigned short *vals, int len)
{
unsigned short max = 0;
int maxpos = 0;
static int oldmaxpos = 0;
static int maxcnt = 0;
static int lastdiff = 0;
int pskfound=0;
static int swait = 0;

    if(swait > 0)
    {
        swait--;
        return;
    }

    //printf("captured %d vals, check %ld with offset:%lld\n",len,CW_BEACON,BEACON_OFFSET);
    //printf("search peak in: %lld %lld\n",BEACON_OFFSET-BEACON_LOCKRANGE,BEACON_OFFSET+BEACON_LOCKRANGE);
    for(int pos=(BEACON_OFFSET-BEACON_LOCKRANGE); pos<(BEACON_OFFSET+BEACON_LOCKRANGE); pos++)
    {
        // measure the peak value for 1s
        if(vals[pos] > max)
        {
            max = vals[pos];
            maxpos = pos;
        }
    }
    //printf("Maximum found at idx: %d\n",maxpos);
    
    int pskcenter = maxpos + PSK_CW_OFFSET;
    int lowsig = 0;
    int highsig = 0;
    int sig = 0;
    // measure the central signal and the lower and upper range of the PSK beacon
    for(int pos=(pskcenter-PSK_BEACON_LOCKRANGE); pos<(pskcenter-1); pos++)
        lowsig += vals[pos];
    lowsig /= (PSK_BEACON_LOCKRANGE-1);
    
    for(int pos=(pskcenter+1); pos<(pskcenter+PSK_BEACON_LOCKRANGE); pos++)
        highsig += vals[pos];
    highsig /= (PSK_BEACON_LOCKRANGE-1);
    
    sig = vals[pskcenter];
    
    // we see the PSK beacon, if lowsig and highsig is at least double of sig
    if((lowsig/2) > sig && (highsig/2) > sig)
    {
        //printf("%05d %05d %05d  PSK Beacon found\n",lowsig,sig,highsig);
        pskfound = 1;
    }
    
    // check if same position is detected for x times
    if(maxpos != oldmaxpos || pskfound == 0)
    {
        oldmaxpos = maxpos;
        maxcnt = 0;
    }
    else
    {
        maxcnt++;
        if(maxcnt >= 2)
        {
            // diff to expected position
            int diff = BEACON_OFFSET - maxpos;
            
            if(diff != 0 && autosync == 1)
            {
                lastdiff += diff;
                int qrgoffset = lastdiff * NB_HZ_PER_PIXEL;
                
                printf("Beacon found at pos:%d diff:%d lastdiff:%d -> %d. Retuning data\n",maxpos,diff,lastdiff,qrgoffset);
                
                newrf = qrgoffset;
                setrfoffset = 1;
                rflock = 0;
                
                // wait a bit for next beacon check to give the SDR a chance to set the new qrg
                swait = 10;
            }
            else
            {
                rflock = 1;
            }
        }
    }
}

#endif
