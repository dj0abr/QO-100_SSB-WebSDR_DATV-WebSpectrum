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
#include "timing.h"
#include "websocketserver.h"
#include "setqrg.h"
#include "ssbdemod.h"

int rflock = 0;

// not used for WB Transponder
#ifndef WIDEBAND

void bcnLock(uint16_t *vals, int len);

fftw_complex *din = NULL;				// input data for  fft, output data from ifft
fftw_complex *cpout = NULL;	            // ouput data from fft, input data to ifft
fftw_plan plan = NULL;
int din_idx = 0;

int audio_cnt[MAX_CLIENTS];
int16_t b16samples[MAX_CLIENTS][AUDIO_RATE];
int audio_idx[MAX_CLIENTS];
int16_t b16samples[MAX_CLIENTS][AUDIO_RATE];
int b16idx[MAX_CLIENTS];


void init_fssb()
{
    char fn[300];
	sprintf(fn, "fssb_wisdom2a");	// wisdom file for each capture rate

	fftw_import_wisdom_from_filename(fn);
  
    din   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * NB_FFT_LENGTH);
	cpout = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * NB_FFT_LENGTH);

    plan = fftw_plan_dft_1d(NB_FFT_LENGTH, din, cpout, FFTW_FORWARD, FFTW_MEASURE);
    
    init_ssbdemod();

    fftw_export_wisdom_to_filename(fn);
    
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        audio_cnt[i] = 0;
        audio_idx[i] = 0;
    }
}

// gain correction
// opper big waterfall
int ssb_gaincorr_rtl = 2000; // if wfsamp[] overflows 16 bit then make this value higher
int small_gaincorr_rtl = 2000;

// lower waterfall
int ssb_gaincorr_sdrplay = 1000;
int small_gaincorr_sdrplay = 1000;
int ex=0;
void fssb_sample_processing(int16_t *xi, int16_t *xq, int numSamples)
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
            
            // this fft has generated NB_FFT_LENGTH bins in cpout
            #define DATASIZE ((NB_FFT_LENGTH/2)/NB_OVERSAMPLING)    // (180.000/2)/10 = 9000 final values
            uint16_t wfsamp[DATASIZE];
            int idx = 0;
            double real,imag;
            int wfbins;
            
            // we do not use the possible lower/upper half of the FFT because the Nyquist frequency
            // is in the middle and makes an awful line in the middle of the waterfall
            // therefore we use the double sampling frequency and display to lower half only
            for(wfbins=0; wfbins<(NB_FFT_LENGTH/2); wfbins+=NB_OVERSAMPLING)
            {
                if(idx >= DATASIZE) break; // all wf pixels are filled
                
                // get the maximum fft-bin for one final value
                double maxv = -99999;
                for(int bin10=0; bin10<NB_OVERSAMPLING; bin10++)
                {
                    real = cpout[wfbins+bin10][0];
                    imag = cpout[wfbins+bin10][1];
                    double v = sqrt((real * real) + (imag * imag));
                    if(v > maxv) maxv = v;
                }

                // level correction
                if(hwtype == 2) maxv /= ssb_gaincorr_rtl;
                else            maxv /= ssb_gaincorr_sdrplay;
                 
                // move level correction value close to maximum level
                if(maxv > 10000) 
                {
                    maxv = 10000;
                    if(hwtype == 2) ssb_gaincorr_rtl+=100;
                    else            ssb_gaincorr_sdrplay+=100;
                }
                
                wfsamp[idx++] = (uint16_t)maxv;
            }
            // here war have wfsamp filled with DATASIZE values
            
            bcnLock(wfsamp,DATASIZE);       // make Beacon Lock

            // left-margin-frequency including clicked-frequency
            unsigned int realrf = TUNED_FREQUENCY - newrf;
            
            // loop through all clients for the waterfalls and the SSB decoder
            for(int client=0; client<MAX_CLIENTS; client++)
            {
                // big waterfall, the same for all clients
                drawWF( WFID_BIG,                   // Waterfall ID
                        wfsamp,                     // FFT output data
                        realrf,            			// frequency of the SDR 
                        WF_RANGE_HZ,                // total width of the fft data in Hz (900.000)
                        NB_HZ_PER_PIXEL,            // Hz/pixel (100)
                        DISPLAYED_FREQUENCY_KHZ,   // frequency of the left margin of the waterfall
                        client);                        // client ID, -1 ... to all clients
                
                // for the SMALL waterfall we need 1500 (WF_WIDTH) bins in a range of 15.000 Hz
                // 
                
                // starting at the current RX frequency - 15kHz/2 (so the RX qrg is in the middle)
                // foffset is the RX qrg in Hz
                // so we need the bins from (foffset/10) - 750 to (foffset/10) + 750
                int start = ((foffset[client])/10) - 750;
                int end = ((foffset[client])/10) + 750;

                if(start < 0) start = 0;
                if(end >= NB_FFT_LENGTH) end = NB_FFT_LENGTH-1;
                
                uint16_t wfsampsmall[WF_WIDTH];
                idx = 0;

                for(wfbins=start; wfbins<end; wfbins++)
                {
                    real = cpout[wfbins][0];
                    imag = cpout[wfbins][1];
                    double dm = sqrt((real * real) + (imag * imag));
                    
                    // correct level
                    if(hwtype == 2) dm /= small_gaincorr_rtl;
                    else            dm /= small_gaincorr_sdrplay;
                    
                    if(dm > 10000) 
                    {
                        dm = 10000;
                        if(hwtype == 2) small_gaincorr_rtl+=100;
                        else            small_gaincorr_sdrplay+=100;
                    }
                    wfsampsmall[idx++] = (uint16_t)dm;
                }

                drawWF( WFID_SMALL,                 // Waterfall ID
                        wfsampsmall,                // FFT output data
                        realrf,            			// frequency of the SDR 
                        15000,               		// total width of the fft data in Hz (in this case 8.000.000)
                        10,			                // Hz/pixel
                        DISPLAYED_FREQUENCY_KHZ + (foffset[client])/1000, // frequency of the left margin of the waterfall
                        client);                    // logged in client index
                
            }
            
            ssbdemod(cpout);
        }
    }
}

// CW Beacon
// calculate the position (offset in the FFT output values) of the CW beacon
// NB_FFT_LENGTH/2 goes over a range of WF_RANGE_HZ Hz starting at DISPLAYED_FREQUENCY_KHZ
// with a resolution of 10Hz per FFT value
#define BEACON_OFFSET   (((long long)CW_BEACON - (long long)DISPLAYED_FREQUENCY_KHZ * (long long)1000L) / ((long long)NB_HZ_PER_PIXEL))
#define BEACON_LOCKRANGE        (long long)1000     // check beacon QRG +- lockrange (1=100Hz)

// PSK Beacon
// has a width of +-600Hz
#define PSK_BEACON_OFFSET   (((long long)PSK_BEACON - (long long)DISPLAYED_FREQUENCY_KHZ * (long long)1000L) / ((long long)NB_HZ_PER_PIXEL))
#define PSK_BEACON_LOCKRANGE        (long long)3     // check beacon QRG +- lockrange (1=100Hz)
#define PSK_CW_OFFSET (PSK_BEACON_OFFSET - BEACON_OFFSET)

void bcnLock(uint16_t *vals, int len)
{
uint16_t max = 0;
int maxpos = 0;
static int oldmaxpos = 0;
static int maxcnt = 0;
//static int lastdiff = 0;
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
    //printf("Maximum found at idx: %d. PSKoffset:%lld\n",maxpos,PSK_CW_OFFSET);
    
    int pskcenter = maxpos + PSK_CW_OFFSET;
    //printf("PSKcenter at%d\n",pskcenter);
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
    
    //printf("l,c,h: %05d %05d %05d\n",lowsig,sig,highsig);
    
    // we see the PSK beacon, if lowsig and highsig is at least double of sig
    if(((lowsig*2)/3) > sig && ((highsig*2)/3) > sig)
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
                int qrgoffset = diff * NB_HZ_PER_PIXEL;
                
                int maxabw = 2;
                if(hwtype == 2) maxabw = 12;

                //printf("*Beacon found at pos:%d diff:%d -> %d\n",maxpos,diff,qrgoffset);

                if(abs(diff) > maxabw)
                {
                    printf("Beacon found at pos:%d diff:%d -> %d\n",maxpos,diff,qrgoffset);
                    newrf += qrgoffset;
                    setrfoffset = 1;
                    rflock = 0;
                }
                
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
