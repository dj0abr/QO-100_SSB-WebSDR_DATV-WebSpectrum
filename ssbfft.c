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
* this function gets samples with a rate of 1.800.000 kS/s (900kHz bandwidth)
* we need an FFT with a resolution of 10 Hz per bin
* so we do the FFT every 90.000 samples (10 times per second)
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
#include "sdrplay.h"
#include "rtlsdr.h"
#include "setup.h"
#include "beaconlock.h"

// not used for WB Transponder
#ifndef WIDEBAND

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
	sprintf(fn, "fssb_wisdom2b");	// wisdom file for each capture rate
    
    int numofcpus = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.
    if(numofcpus > 1)
    {
        printf("found %d cores, running FFT in multithreading mode\n",numofcpus);
        fftw_init_threads();
        fftw_plan_with_nthreads(numofcpus);
    }

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

// info for one line
double binline[NB_FFT_LENGTH / 2];
double binline_shifted[NB_FFT_LENGTH / 2];

void fssb_sample_processing(int16_t *xi, int16_t *xq, int numSamples)
{
double real, imag;

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
            
            // the FFT delivers NB_FFT_LENGTH values (bins)
            // but we only use the first half of them, which is the full range with the 
            // requested resolution of 10 Hz per bin
            
            // now calculate the absolute level from I and Q values
            // and measure the max value
            for(int i=0; i<(NB_FFT_LENGTH/2); i++)
            {
                real = cpout[i][0];
                imag = cpout[i][1];
                binline[i] = sqrt((real * real) + (imag * imag));
            }
            
            // binline now has the absolute spectrum levels of one waterfall line with 10Hz resolution
            
            if(offset_tuned == 1)
            {
                // the tuner has been corrected already
                // now make a fine correction by shifting the spectrum
                bcnLock2(binline);
            }
            
            // this fft has generated NB_FFT_LENGTH bins in cpout
            #define DATASIZE ((NB_FFT_LENGTH/2)/NB_OVERSAMPLING)    // (180.000/2)/10 = 9000 final values
            uint16_t wfsamp[DATASIZE];
            int idx = 0;
            int wfbins;
            
            // we do not use the possible lower/upper half of the FFT because the Nyquist frequency
            // is in the middle and makes an awful line in the middle of the waterfall
            // therefore we use the double sampling frequency and display to lower half only
            
            // fft output in cpout from index 0 to NB_FFT_LENGTH/2 which are 90.000 values
            // with a resolution of 10 Hz per value
            
            // frequency fine correction
            // not all tuners can be set to 1Hz precision, i.e. the RTLsdr has only huge steps
            // this must be corrected here by shifting the cpout
            // Resolution: 10 Hz, so every single shift means 10 Hz correction
            
            int corr = offqrg; // correction shift in 10Hz steps
            int corr_start = corr;
            int corr_end = (NB_FFT_LENGTH/2) + corr;
            if(corr_start < 0) corr_start = -corr_start;
            if(corr_end >= (NB_FFT_LENGTH/2)) corr_end = (NB_FFT_LENGTH/2);
            int corr_len = corr_end - corr_start;

            if(corr == 0)
            {
                // no qrg correction
                memcpy(&(binline_shifted[0]), &(binline[0]), sizeof(double) * NB_FFT_LENGTH / 2);
            }
            else
            {
                memset(&(binline_shifted[0]), 0, sizeof(double) * NB_FFT_LENGTH / 2);
                if(corr > 0)
                    memcpy(&(binline_shifted[corr_start]), &(binline[0]), sizeof(double) * corr_len);
                else
                    memcpy(&(binline_shifted[0]), &(binline[corr_start]), sizeof(double) * corr_len);
            }
            
            // kill spike on 10490 +- 900 Hz
            for(int q=(75000-90); q<(75000+90); q++)
                binline_shifted[q] = binline_shifted[q+2000];

            for(wfbins=0; wfbins<(NB_FFT_LENGTH/2); wfbins+=NB_OVERSAMPLING)
            {
                if(idx >= DATASIZE) break; // all wf pixels are filled
                
                // get the maximum fft-bin for one final value
                double maxv = -99999;
                for(int bin10=0; bin10<NB_OVERSAMPLING; bin10++)
                {
                    double v = binline_shifted[wfbins+bin10];
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
            // here we have wfsamp filled with DATASIZE values
            
            if(offset_tuned == 0)
                bcnLock1(wfsamp,DATASIZE);       // make Beacon Lock

            // left-margin-frequency including clicked-frequency
            unsigned int realrf = tuned_frequency - newrf;
            
            // loop through all clients for the waterfalls and the SSB decoder
            for(int client=0; client<MAX_CLIENTS; client++)
            {
                // big waterfall, the same for all clients
                drawWF( WFID_BIG,                   // Waterfall ID
                        wfsamp,                     // FFT output data
                        realrf,            			// frequency of the SDR 
                        WF_RANGE_HZ,                // total width of the fft data in Hz (900.000)
                        NB_HZ_PER_PIXEL,            // Hz/pixel (100)
                        LEFT_MARGIN_QRG_KHZ,   // frequency of the left margin of the waterfall
                        client);                        // client ID, -1 ... to all clients
                
                // for the SMALL waterfall we need 1500 (WF_WIDTH) bins in a range of 15.000 Hz
                
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
                    double dm = binline_shifted[wfbins];
                    
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
                        LEFT_MARGIN_QRG_KHZ + (foffset[client])/1000, // frequency of the left margin of the waterfall
                        client);                    // logged in client index
                
            }
            
            ssbdemod(cpout, corr_start);
        }
    }
}

#endif
