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
* wb_fft.c ... 
* 
* fft for wide spectrum
* 
*/

#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "qo100websdr.h"
#include "wf_univ.h"
#include "websocketserver.h"
#include "setup.h"

#define WB_SRATE          SDR_SAMPLE_RATE               // full speed without decimation (10M)
#define WB_RESOLUTION     1000                          // Hz per FFT value
#define WB_FFT_LENGTH     (WB_SRATE / WB_RESOLUTION)    // = 10.000

fftw_complex *wb_din = NULL;				// input data for  fft, output data from ifft
fftw_complex *wb_cpout = NULL;	            // ouput data from fft, input data to ifft
fftw_plan wb_plan = NULL;
int wb_din_idx = 0;


void init_fwb()
{
    char fn[300];
	sprintf(fn, "wb_wisdom");	// wisdom file for each capture rate
    
    int numofcpus = sysconf(_SC_NPROCESSORS_ONLN); // Get the number of logical CPUs.
    if(numofcpus > 1)
    {
        printf("found %d cores, running FFT in multithreading mode\n",numofcpus);
        fftw_init_threads();
        fftw_plan_with_nthreads(numofcpus);
    }

	fftw_import_wisdom_from_filename(fn);
  
    wb_din   = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * WB_FFT_LENGTH);
	wb_cpout = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * WB_FFT_LENGTH);

    wb_plan = fftw_plan_dft_1d(WB_FFT_LENGTH, wb_din, wb_cpout, FFTW_FORWARD, FFTW_MEASURE);

    fftw_export_wisdom_to_filename(fn);
}

int picture_div = WF_RANGE_HZ / WF_WIDTH / 1000;   // = 5 for 8M bandwidth and 1600 pixels
int gaincorr = 330; // if wfsamp[] overflows 16 bit then make this value higher

void wb_sample_processing(short *xi, short *xq, int numSamples)
{
    // copy the samples into the FFT input buffer
    for(int i=0; i<numSamples; i++)
    {
        wb_din[wb_din_idx][0] = xi[i];
		wb_din[wb_din_idx][1] = xq[i];
        wb_din_idx++;
        
        // check if the fft input buffer is filled
        if(wb_din_idx >= WB_FFT_LENGTH)
        {
            wb_din_idx = 0;

            // calc WF only wftimes times to slow down the load
            // make it as slow as convenient, or it will eat a lot of
            // CPU in the server and also in the browser
            // a value between 50 and 100 should be ok
            static int wftimes = 0;
            
            #ifdef SDR_PLAY
                static int divider = 50;
            #endif
            #ifdef PLUTO
                static int divider = 10;
            #endif
            if(++wftimes > divider)
            {
            
                // the buffer is full, now lets execute the fft
                fftw_execute(wb_plan);
                
                unsigned short wfsamp[WF_WIDTH];
                int idx = 0;
                double real,imag;
                int wfbins;

                /*
                 * SDRPLAY: (10 MHz Sample Rate)
                 * =============================
                * the FFT generates Fmid to Fmid+5MHz followed by Fmid-5MHz to Fmid
                * so we get WB_FFT_LENGTH fft values (bins). In this case we get 10.000
                * values
                * 
                * the samplerate is 10MHz but the bandwidth is only 8MHz, so we can cut 1 MHz at the beginning and at the end
                * this results in 8000 usable bins.
                * Also we have to put this into the 1600 pixels, which is one pixel for 5 bins.
                * 
                * So we cut 1000 bins (1 MHz) at the beginning and 1000 (1 MHz)at the end.
                * The total width of the waterfall is now 10MHz - 2*1MHz = 8,000 MHz.
                * The base frequency Fmid should be on 10.495.5 GHz, so the range is
                * 10.491.5 (=LEFT_MARGIN_QRG_KHZ) to 10.499.5 GHz
                * 
                * PLUTO: (16 MHz Sample Rate)
                * ===========================
                * 
                * the FFT generates Fmid to Fmid+8MHz followed by Fmid-8MHz to Fmid
                * so we get WB_FFT_LENGTH fft values (bins). In this case we get 16.000
                * values
                * 
                * the samplerate is 16MHz but the picture width is only 8MHz, so we can cut
                * 4 MHz at the beginning and at the end
                * this results in 8000 usable bins.
                * Also we have to put this into the 1600 pixels, which is one pixel
                * for 5 bins.
                * 
                */
                
                #ifdef SDR_PLAY
                    static int cutoff = 1000;
                #endif
                #ifdef PLUTO
                    static int cutoff = 4000;
                #endif
                    
                // left half ob the picture
                #ifdef PLUTO
                // the Pluto has a peak at the Zero-Frequency, wipe this out
                int midclean = 20;
                int x=0;
                for(wfbins=(WB_FFT_LENGTH-midclean); wfbins<(WB_FFT_LENGTH); wfbins++)
                {
                    wb_cpout[wfbins][0] = wb_cpout[WB_FFT_LENGTH-midclean-x][0];
                    wb_cpout[wfbins][1] = wb_cpout[WB_FFT_LENGTH-midclean-x][1];
                    x++;
                }
                #endif
                    
                int minleft = WB_FFT_LENGTH/2+cutoff;
                for(wfbins=minleft; wfbins<(WB_FFT_LENGTH); wfbins+=picture_div)
                {
                    if(idx >= WF_WIDTH) break; // all wf pixels are filled
                    
                    double maxv = -99999;
                    for(int bin10=0; bin10<picture_div; bin10++)
                    {
                        real = wb_cpout[wfbins+bin10][0];
                        imag = wb_cpout[wfbins+bin10][1];
                        double v = sqrt((real * real) + (imag * imag));
                        if(v > maxv) maxv = v;
                    }
                    
                    #ifdef SDR_PLAY
                    // the edges of the spectrum are in the SDRs filter range already
                    // compensate by raising the level at the edges
                    if(wfbins < (minleft+400))
                    {
                        int ap = 400 - (wfbins - minleft);
                        ap /= 6;
                        maxv *= (210 + ap);
                        maxv /= 200;
                    }
                    #endif

                    // level correction
                    maxv /= gaincorr;
                    if(maxv > 65535) printf("maxv overflow, rise gaincorr\n");
                    wfsamp[idx] = (unsigned short)maxv;

                    idx++;
                }

                // right half ob the picture
                #ifdef PLUTO
                for(wfbins=0; wfbins<midclean; wfbins++)
                {
                    wb_cpout[wfbins][0] = wb_cpout[midclean+wfbins][0];
                    wb_cpout[wfbins][1] = wb_cpout[midclean+wfbins][1];
                }
                #endif
                
                int maxright = WB_FFT_LENGTH/2-cutoff;
                for(wfbins=0; wfbins<maxright; wfbins+=picture_div)
                {
                    if(idx >= WF_WIDTH) break; // all wf pixels are filled
                    
                    double maxv = -99999;
                    for(int bin10=0; bin10<picture_div; bin10++)
                    {
                        real = wb_cpout[wfbins+bin10][0];
                        imag = wb_cpout[wfbins+bin10][1];
                        double v = sqrt((real * real) + (imag * imag));
                        if(v > maxv) maxv = v;
                    }
                    
                    #ifdef SDR_PLAY
                    // the edges of the spectrum are in the SDRs filter range already
                    // compensate by raising the level at the edges
                    if(wfbins > (maxright-400))
                    {
                        int ap = wfbins - (maxright-400);
                        ap /= 6;
                        maxv *= (210 + ap);
                        maxv /= 200;
                    }
                    #endif

                    // level correction
                    maxv /= gaincorr;
                    if(maxv > 65535) printf("maxv overflow, rise gaincorr\n");
                    wfsamp[idx] = (unsigned short)maxv;

                    idx++;
                }
                
                for(int client=0; client<MAX_CLIENTS; client++)
                {
                    drawWF( WFID_BIG,                   // Waterfall ID
                            wfsamp,                     // FFT output data
                            tuned_frequency,            // frequency of the SDR 
                            WF_RANGE_HZ,                // total width of the fft data in Hz (in this case 8.000.000)
                            WF_RANGE_HZ/WF_WIDTH,       // Hz/pixel
                            LEFT_MARGIN_QRG_KHZ,client);   // frequency of the left margin of the waterfall
                }
                
                wftimes = 0;
            }
        }
    }
}

