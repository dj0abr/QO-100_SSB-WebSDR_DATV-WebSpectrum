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

// wide lockrange around the lower F1A beacon
#define CW_BEACON_INDEX (((int)((long long)CW_BEACON/1000) - LEFT_MARGIN_QRG_KHZ)*100)
#define BCN_WIDERANGE   (200000/10) // +-100 kHz = 200 kHz wide lock range
#define BCN_WIDESTART   (CW_BEACON_INDEX - BCN_WIDERANGE/2)
#define BCN_WIDEEND     (CW_BEACON_INDEX + BCN_WIDERANGE/2)
#define BCN_ARRLEN      (BCN_WIDERANGE*sizeof(double))

// averaging array (circular buffer)
#define ARRLEN  25     // averages ARRLEN*10 = 0,25s
double valarr[ARRLEN][BCN_WIDERANGE];
double midarr[BCN_WIDERANGE];
int arridx = 0;

void bcnLock(double *vals)
{
    // vals: RXed values, 90.000 values for 900kHz with a resolutioin of 10 Hz
    
    // add the new value to the circular buffer
    memcpy(&(valarr[arridx][0]), &(vals[BCN_WIDESTART]), BCN_ARRLEN);
    if(++arridx == ARRLEN) arridx=0;
    
    // calc avarage values and min and max value
    memset(midarr,0,BCN_ARRLEN);
    for(int i=0; i<BCN_WIDERANGE; i++)
    {
        for(int ln=0; ln<ARRLEN; ln++)
            midarr[i] += valarr[ln][i];
    }
    
    double max = 0;
    double min = 1e12;
    for(int i=0; i<BCN_WIDERANGE; i++)
    {
        midarr[i] /= ARRLEN;
        if(midarr[i] > max) max = midarr[i];
        if(midarr[i] < min) min = midarr[i];
    }
    
    // check for beacon only if max > mix*20, otherwise no antenna may be connected or no signal
    if(max < (min*20)) return;
    
    // test if right maximum is 400 Hz higher then left maximum (range: 37 to 41)
    // test only values with a significant level
    int fidx = 0;
    double refval = max*3/4;
    static int okcnt = 0;
    for(int i=0; i<BCN_WIDERANGE; i++)
    {
        if(midarr[i] > refval && 
           (midarr[i+38] > refval ||
            midarr[i+39] > refval ||
            midarr[i+40] > refval ||
            midarr[i+41] > refval))
        {
            fidx = i;
            //printf("match %d: %d\n",okcnt,i);
        }
    }
    
    if(fidx > 0) okcnt++;
    else         okcnt = 0;
    
    fidx += 2;  // correction value, looks like we need that
    
    int offset = BCN_WIDERANGE/2 - fidx;
    
    static int old_offset = 0;
    if(okcnt > 4)  // if we found the beacon 4 times
    {
        if(offset < (old_offset-1) || offset > (old_offset+1))
        {
            if(abs(offset) < 100)
            {
                // shift spectrum
                printf("BCN found: %d fine correct offset:%d Hz\n",fidx,offset*10);
                offqrg = offset;
                rflock = 1;
            }
            else
            {
                // re tune
                printf("BCN found: %d re-tune correct offset:%d Hz\n",fidx,offset*10);
                offqrg =  0;
                    
                if(hwtype == 1)
                {
                    #ifdef SDR_PLAY
                    setTunedQrgOffset(offset*10);
                    offset_tuned = 1;
                    notfound = 0;
                    #endif
                }
                
                if(hwtype == 2)
                {
                    #ifndef WIDEBAND
                    rtlsetTunedQrgOffset(offset*10);
                    offset_tuned = 1;
                    notfound = 0;
                    #endif
                }
            }
            old_offset = offset;
        }
    }
    
    /*
    for(int i=(BCN_WIDERANGE/2-5); i<(BCN_WIDERANGE/2+90); i++)
    {
        if(midarr[i] > 0) printf("%d",i);
        else printf("-");
    }
    printf("\n");
    */
}

#endif //WIDEBAND
