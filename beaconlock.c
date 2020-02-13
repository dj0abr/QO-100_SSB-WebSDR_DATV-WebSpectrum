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
* beaconlock.c ... 
* 
* looks for the beacons and corrects the tuning
* 
*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <fftw3.h>
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

int rflock = 0;
int offqrg = 0;
int offset_tuned = 0;   // 1...tuner has been retuned to correct an offset 
int notfound = 0;

// not used for WB Transponder
#ifndef WIDEBAND

// CW Beacon - Step-1: this function changes the tuner frequency. Max. precision: 100Hz and with the RTLsdr
// about 400 Hz

// calculate the position (offset in the FFT output values) of the CW beacon
// NB_FFT_LENGTH/2 goes over a range of WF_RANGE_HZ Hz starting at LEFT_MARGIN_QRG_KHZ
// with a resolution of 10Hz per FFT value
#define BEACON_OFFSET   (((long long)CW_BEACON - (long long)LEFT_MARGIN_QRG_KHZ * (long long)1000L) / ((long long)NB_HZ_PER_PIXEL))
#define BEACON_LOCKRANGE        (long long)1000     // check beacon QRG +- lockrange (1=100Hz)

// PSK Beacon
// has a width of +-600Hz
#define PSK_BEACON_OFFSET   (((long long)PSK_BEACON - (long long)LEFT_MARGIN_QRG_KHZ * (long long)1000L) / ((long long)NB_HZ_PER_PIXEL))
#define PSK_BEACON_LOCKRANGE        (long long)6     // check beacon QRG +- lockrange (1=100Hz)
#define PSK_CW_OFFSET (PSK_BEACON_OFFSET - BEACON_OFFSET)

void bcnLock1(uint16_t *vals, int len)
{
uint16_t max = 0;
int maxpos = 0;
static int oldmaxpos = 0;
static int maxcnt = 0;
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
    //printf("Maximum found at idx: %d = %.6f MHz\n",maxpos,((double)LEFT_MARGIN_QRG_KHZ*1000.0+(double)maxpos*100.0)/1e6);
    
    int pskcenter = maxpos + PSK_CW_OFFSET;
    //printf("searching PSK-beacon center at %d = %.6f MHz\n",pskcenter,((double)LEFT_MARGIN_QRG_KHZ*1000.0+(double)pskcenter*100.0)/1e6);
    int lowsig = 0;
    int highsig = 0;
    int sig = 0;
    // measure the central signal and the lower and upper range of the PSK beacon
    for(int pos=(pskcenter-PSK_BEACON_LOCKRANGE-1); pos<(pskcenter-1); pos++)
        lowsig += vals[pos];
    lowsig /= (PSK_BEACON_LOCKRANGE-1);
    
    for(int pos=(pskcenter+2); pos<(pskcenter+PSK_BEACON_LOCKRANGE+1); pos++)
        highsig += vals[pos];
    highsig /= (PSK_BEACON_LOCKRANGE-1);
    
    sig = vals[pskcenter];
    
    //printf("l,c,h: %05d %05d %05d : %05d %05d %05d\n",lowsig,sig,highsig,vals[pskcenter-3],vals[pskcenter],vals[pskcenter+3]);
    //printf("l,c,h: %05d %05d %05d\n",lowsig,sig,highsig);
    
    // we see the PSK beacon, if lowsig and highsig is at least double of sig
    if(((lowsig*2)/4) > sig && ((highsig*2)/4) > sig)
    {
        //printf("PSK Beacon found\n");
        pskfound = 1;
    }
    
    // check if same position is detected for check_times times
    int check_times = 6;
    if(maxpos != oldmaxpos || pskfound == 0)
    {
        oldmaxpos = maxpos;
        maxcnt = 0;
    }
    else
    {
        maxcnt++;
        if(maxcnt >= check_times)
        {
            // diff to expected position
            int diff = BEACON_OFFSET - maxpos;
            
            if(diff != 0 && autosync == 1)
            {
                int qrgoffset = diff * NB_HZ_PER_PIXEL;
                
                int maxabw = 4;
                //if(hwtype == 2) maxabw = 16;

                printf("*Beacon found at pos:%d diff:%d -> %d Hz\n",maxpos,diff,qrgoffset);

                if(abs(diff) > maxabw)
                {
                    printf("Beacon found at pos:%d diff:%d -> %d(+%d) = %d\n",maxpos,diff,newrf,qrgoffset,qrgoffset+newrf);
                    newrf += qrgoffset;
                    rflock = 0;

                    // store the offset in Hz for the frequency correction
                    //offqrg = qrgoffset;                    
                    offqrg =  0;
                    
                    if(hwtype == 1)
                    {
                        #ifdef SDR_PLAY
                        setTunedQrgOffset(qrgoffset);
                        offset_tuned = 1;
						notfound = 0;
                        #endif
                    }
                    
                    if(hwtype == 2)
                    {
                        #ifndef WIDEBAND
                        rtlsetTunedQrgOffset(qrgoffset);
                        offset_tuned = 1;
						notfound = 0;
                        #endif
                    }
                }
                else
                {
                    if(offset_tuned == 0)
                        printf("small offset, do not re-tune, use software correction\n");
                    offset_tuned = 1;
					notfound = 0;
                }
                
                // wait a bit for next beacon check to give the SDR a chance to set the new qrg
                swait = 10;
            }
            else
            {
                offset_tuned = 0;
                rflock = 1;
            }
        }
    }
}

// do a fine tuning
// calculate the offset of the beacon in 10Hz steps
// the tuner has been corrected already, so the beacon should be within +- 500 Hz

// the becons should be on this index of the fft output values in cpout
#define BEACON_10HZ_OFFSET   (((long long)CW_BEACON - (long long)LEFT_MARGIN_QRG_KHZ * (long long)1000L) / ((long long)NB_RESOLUTION))

// search for the beacon +- LOCKRANGE_10HZ, which is 1kHz
#define LOCKRANGE_10HZ  (long long)(5000/NB_RESOLUTION) 

void bcnLock2(double *binline)
{
    int start = BEACON_10HZ_OFFSET - LOCKRANGE_10HZ;
    int end = BEACON_10HZ_OFFSET + LOCKRANGE_10HZ;

    double maxval = -9999;
    int maxpos = 0;
    for(int i=start; i<end; i++)
    {
        double v = binline[i];
        if(v > maxval) 
        {
            maxval = v;
            maxpos = i;
        }
    }
    //printf("fine max: %d\n",maxpos);
    
    // accept fine max if the same value for multiple times 
    static int lastmax = 0;
    static int lastoffset = 0;
    static int maxcnt = 0;
	if(lastmax == maxpos)
    {
        if(++maxcnt > 5)
        {
			notfound = 0;
            int offset = ((int)BEACON_10HZ_OFFSET - maxpos);
            if(abs(offset) > LOCKRANGE_10HZ)
            {
                printf("very wide offset, use tuner correction\n");
                offqrg = 0;
                rflock = 0;
                lastmax = 0;
                maxcnt = 0;
                offset_tuned = 0;
                return;
            }
            
            // correct only if >10Hz difference to avoid jumping around the center value
            if(maxpos < ((int)BEACON_10HZ_OFFSET-1) || maxpos > ((int)BEACON_10HZ_OFFSET+1))
            {
                if(offset != lastoffset)
                {
                    lastoffset = offset;
                    //printf("fine correct offset: %d Hz\n",(maxpos - (int)BEACON_10HZ_OFFSET)*10);
                    offqrg = offset;
                    rflock = 1;
                }
            }
        }
    }
    else
	{
        maxcnt = 0;
		notfound++;
		if(notfound > 100)
		{
			printf("beacon not found, use tuner correction\n");
			notfound = 0;
			offset_tuned = 0;
		}
	}

    lastmax = maxpos;
}

#endif //WIDEBAND
