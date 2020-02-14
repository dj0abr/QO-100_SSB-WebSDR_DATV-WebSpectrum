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
#include "qo100websdr.h"
#include "sdrplay.h"
#include "rtlsdr.h"
#include "cat.h"
#include "websocket/websocketserver.h"
#include "setqrg.h"
#include "fifo.h"
#include <fftw3.h>
#include "ssbdemod.h"
#include "setup.h"

int foffset[MAX_CLIENTS];    // audio offset to tuned frequency
int ssbmode = 1;    // 0=LSB 1=USB
int setrfoffset = 0;
unsigned int newrf = 0;
int autosync = 1;   // always on

// called in the main loop
// checks if a new set-frequency command arrived
void set_frequency()
{
unsigned int frdiff;
static int f=1;
int off;
static int last_setIcomQRG = 0;

    if(f)
    {
        f=0;
        for(int i=0; i<MAX_CLIENTS; i++)
            foffset[i] = 300000;
    }

    // look if user command available
    USERMSG rx_usermsg;
    int len = read_pipe(FIFO_USERCMD, (unsigned char *)(&rx_usermsg), sizeof(USERMSG));
    if(len == 0) return;
    
    //printf("user command:%d from client:%d parameter:%d\n",rx_usermsg.command,rx_usermsg.client,rx_usermsg.para);

    switch (rx_usermsg.command)
    {
        case 1: 
                #ifdef WIDEBAND
                    foffset[rx_usermsg.client] = rx_usermsg.para * 5250;
                #else
                    foffset[rx_usermsg.client] = rx_usermsg.para;
                    printf("freqval:%d foffset:%d\n",rx_usermsg.para,foffset[rx_usermsg.client]);
                #endif
                printf("new QRG offset: %d\n",foffset[rx_usermsg.client]);
                break;
                
        case 2: foffset[rx_usermsg.client] += (rx_usermsg.para * 10);
                break;
        
        case 4: ssbmode = rx_usermsg.para;
                printf("set mode: %s\n",ssbmode?"USB":"LSB");
                break;
                
        case 6: frdiff = rx_usermsg.para;
                if(frdiff == 1) newrf-=100;
                else newrf+=100;
                printf("set tuner qrg: %d\n",frdiff);
                setrfoffset = 1;
                break;
                
        /*case 7: autosync = rx_usermsg.para;
                printf("auto beacon lock: %d\n",autosync);
                break; */
                
        case 8: // Browser sends a new tuner frequency
                newrf = tuned_frequency - rx_usermsg.para;
                printf("set tuner qrg: %d (%d)\n",newrf,tuned_frequency - newrf);
                setrfoffset = 1;
                break;
                
        case 9: // mouse click lower WF 0..1500
                // actual qrg is in the middle at pos=750
                // resolution: 15kHz (10 Hz/pixel)
                //foffset += ((rx_usermsg.para * NB_RESOLUTION) - (WF_WIDTH/2)*NB_RESOLUTION);
                #ifndef WIDEBAND
                foffset[rx_usermsg.client] += (rx_usermsg.para - WF_WIDTH/2) * NB_RESOLUTION;
                printf("new QRG offset: %d\n",foffset[rx_usermsg.client]);
                //downmixer_setFrequency(foffset[rx_usermsg.client],rx_usermsg.client);
                #endif
                break;
                
        case 10:// activate icom CAT interface (open/close serial port)
                useCAT = rx_usermsg.para;
                break;
        case 12:// audio decoder for this client on/off
                #ifndef WIDEBAND
                printf("switch audio %d for client %d\n",rx_usermsg.para,rx_usermsg.client);
                audioon[rx_usermsg.client] = rx_usermsg.para;
                #endif
                break;
        case 13:// set ICOM on double click
                setIcomQRG = rx_usermsg.para;
                last_setIcomQRG = setIcomQRG;
                break;
        case 14:// right click in lower WF
                #ifndef WIDEBAND
                if(last_setIcomQRG > 0)
                {
                    off = (rx_usermsg.para - WF_WIDTH/2) * NB_RESOLUTION;
                    setIcomQRG = last_setIcomQRG + off;
                    printf("off: %d, new qrg:%d\n",off,setIcomQRG);
                    last_setIcomQRG = setIcomQRG;
                }
                #endif
                break;
    }
    
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

void re_set_freq()
{
    if(hwtype == 1)
    {
        #ifdef SDR_PLAY
        reset_Qrg_SDRplay();
        #endif
    }
    
    if(hwtype == 2)
    {
        #ifndef WIDEBAND
        reset_Qrg_RTLsdr();
        #endif
    }
}
