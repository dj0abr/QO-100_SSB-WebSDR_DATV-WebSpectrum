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
* setqrg.c ... all jobs to set the frequency and other parameters
* 
*/

#include <stdio.h>
#include "setqrg.h"
#include "qo100websdr.h"
#include "sdrplay.h"
#include "rtlsdr.h"
#include "downmixer.h"
#include "cat.h"

// 0=do nothing, 1=set to mouse pos 2=increment/decrement
// 3=set a band, 4=set LSB/USB
int setfreq = 0;    
int freqval;        // value for above command

int foffset = 0;    // audio offset to tuned frequency
int ssbmode = 1;    // 0=LSB 1=USB
int filtermode = 1; // 0=1,8k 1=2,4k 2=3,6k
int setrfoffset = 0;
unsigned int newrf = 0;
int autosync = 0;


// called in the main loop
// checks if a new set-frequency command arrived
void set_frequency()
{
unsigned int frdiff;

    switch (setfreq)
    {
        case 1: 
                #ifdef WIDEBAND
                    foffset = freqval * 5250;
                #else
                    foffset = freqval * (WF_RANGE_HZ / WF_WIDTH);
                    downmixer_setFrequency(foffset);
                #endif
                printf("new QRG offset: %d\n",foffset);
                break;
                
        case 2: foffset += (freqval * 10);
                #ifndef WIDEBAND
                downmixer_setFrequency(foffset);
                #endif
                break;
        
        case 4: ssbmode = freqval;
                printf("set mode: %s\n",ssbmode?"USB":"LSB");
                break;
                
        case 5: filtermode = freqval;
                printf("set filter: %d\n",filtermode);
                break;
                
        case 6: frdiff = freqval;
                if(frdiff == 1) newrf-=100;
                else newrf+=100;
                printf("set tuner qrg: %d\n",frdiff);
                setrfoffset = 1;
                break;
                
        case 7: autosync = freqval;
                printf("auto beacon lock: %d\n",autosync);
                break; 
                
        case 8: // Browser sends a new tuner frequency
                newrf = TUNED_FREQUENCY - freqval;
                printf("set tuner qrg: %d (%d)\n",newrf,TUNED_FREQUENCY - newrf);
                setrfoffset = 1;
                break;
                
        case 9: // mouse click lower WF 0..1500
                // actual qrg is in the middle at pos=750
                // resolution: 15kHz (10 Hz/pixel)
                //foffset += ((freqval * NB_RESOLUTION) - (WF_WIDTH/2)*NB_RESOLUTION);
                #ifndef WIDEBAND
                foffset += (freqval - WF_WIDTH/2) * NB_RESOLUTION;
                printf("new QRG offset: %d\n",foffset);
                downmixer_setFrequency(foffset);
                #endif
                break;
                
        case 10:// activate icom CAT interface (open/close serial port)
                useCAT = freqval;
                break;
    }
    setfreq = 0;
    
    if(setrfoffset)
    {
        if(hwtype == 1)
        {
            #ifdef SDR_PLAY
            setTunedQrgOffset(newrf);
            #endif
        }
        
        if(hwtype == 2)
        {
            #ifndef WIDEBAND
            rtlsetTunedQrgOffset(newrf);
            #endif
        }
        setrfoffset = 0;
    }
}
