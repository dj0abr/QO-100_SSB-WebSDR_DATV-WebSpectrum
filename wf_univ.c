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
* wf_univ.c ... drawing routines for waterfalls, using gdlib
* 
* usage:
* drawWF(int id, double *fdata, int cnt, int wpix, int hpix, int _leftqrg, int picwidthHz, int res, int _tunedQRG, char *fn);
* 
* id ... a unique number (used to create a temporary file name, see wf_univ.h)
* fdata ... a double array containing the FFT result
* _leftqrg ... frequency offset of the left margin of the bitmap (usually 0, the first FFT value)
* picwidthHz ... frequency offset of the right margin of the bitmap (usually: FFTRESOLUTION in Hz * width in pixels)
* res ... FFT resolution in Hz per FFT value (= Hz per pixel)
* _tunedQRG ... frequency where the SDR receivers is tuned (used for displaying the frequncy in the title)
* 
* the bitmap is created as a temporary BMP file in the /tmp folder.
* after drawing this is copied to fn
* 
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "wf_univ.h"
#include "websocket/websocketserver.h"
#include "setqrg.h"
#include "civ.h"
#include "cat.h"
#include "setup.h"

void drawWF(int id, unsigned short *fdata, unsigned int _realqrg, int picwidthHz, int res, int _tunedQRG, int client)
{
    // 1 byte: ID counter
    // 1 byte: waterfall ID
    // 4 byte: real qrg of the SDR's hardware tuner in Hz
    // 4 byte: right margin frequency in Hz
    // 4 byte: tuner frequency in Hz
    // 4 byte: resolution Hz per pixel
    // 4 byte: offset to tuner frequency in Hz
    // followed by the dBm data, 1 byte per pixel holding the dBm value
    int right = picwidthHz / res;
    unsigned char wfdata[right*2+50];
    int idx = 0;
    static unsigned char idcnt = 0;
    
    // idcnt ... number of this dataset, used to detect if the dataset was handled already
    wfdata[idx++] = idcnt++;
    // waterfall id, in case we have more waterfalls
    wfdata[idx++] = id;
    
    // real qrg of the SDR's hardware tuner in Hz
     wfdata[idx++] = _realqrg >> 24;
    wfdata[idx++] = _realqrg >> 16;
    wfdata[idx++] = _realqrg >> 8;
    wfdata[idx++] = _realqrg;
    
    // offset of the right margin of the picture in Hz
    // which is the width of the picture in Hz (600000)
    wfdata[idx++] = picwidthHz >> 24;
    wfdata[idx++] = picwidthHz >> 16;
    wfdata[idx++] = picwidthHz >> 8;
    wfdata[idx++] = picwidthHz;
    
    // frequency where the SDR is tuned in Hz
    int tqrg = _tunedQRG;

    wfdata[idx++] = tqrg >> 24;
    wfdata[idx++] = tqrg >> 16;
    wfdata[idx++] = tqrg >> 8;
    wfdata[idx++] = tqrg;

    // the resolution in Hz/pixel
    wfdata[idx++] = res >> 24;
    wfdata[idx++] = res >> 16;
    wfdata[idx++] = res >> 8;
    wfdata[idx++] = res;

    // offset of the RX frequency to the tuner frequency in Hz
    // if a freq was recved via CIV, enter it here
    if(useCAT != 0 && civ_freq != 0)
    {
		// if civ and sdr are on different band, we need to compensate by just using the kHz
		// if it is below 500kHz then it is the TX qrg, in this case add 500 kHz
        // tx_correction is a value which can be used to correct an offset of an external transmitter (ICOM)
        // which is used by CIV to read the frequency
        int corval = 500000 + tx_correction;
		int tf = (tuned_frequency / 1000000) * 1000000;	// tf is the tunded qrg, only MHz
		int kHz = civ_freq - ((civ_freq / 1000000) * 1000000); // only kHz
		if(kHz < 500000) kHz += corval;
        int offset = tf + kHz- tuned_frequency;
        // assign it to local users only
        if(isLocal(client))
            foffset[client] = offset;
    }
    
    wfdata[idx++] = foffset[client] >> 24;
    wfdata[idx++] = foffset[client] >> 16;
    wfdata[idx++] = foffset[client] >> 8;
    wfdata[idx++] = foffset[client];
    
    wfdata[idx++] = get_useranz();  // number of users logged in
    wfdata[idx++] = 0;  // spare
    wfdata[idx++] = 0;  // spare
    wfdata[idx++] = 0;  // spare

    // draw pixel per pixel
    for(int i=0; i<right; i++)
    {
        // fdata is the result of the fft and 16 bit wide
        // first take the MSB then the LSB
        wfdata[idx++] = (unsigned char)(fdata[i] >> 8);
        wfdata[idx++] = (unsigned char)fdata[i];
        if(idx > sizeof(wfdata)) 
        {
            printf("wfdata overflow ! wfdata-idx:%d, max. allowed:%d\n",idx,sizeof(wfdata));
            break;
        }
    }
    
    // wfdata with length cnt is now filled with data
    // give it to the WebSocket Server
    ws_send(wfdata,idx,id, client);
}
