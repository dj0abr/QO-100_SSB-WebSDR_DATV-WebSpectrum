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
* extApp.c ... 
* 
* this function sends the FFT result to Amsat HSmodem via UDP
* 
*/

#include "qo100websdr.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <wchar.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/file.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <math.h>
#include "udp/udp.h"


// NB Transponder
// gets the FFT result for an external application
// wfsamp ... FFT data starting at 10489,250 until 10490,150, Resolution 100 Hz
// len ... 9000
// for NB we need only 10489,475 - 10490,025 = 550kHz

// we do 2kHz per pixel, then we get 275 values
// each pixel is the mean value of 20 fft values

void sendExt(uint16_t *wfsamp, int len)
{
#ifndef AMSATBEACON
    return;
#endif
    
const int start = 2250;
const int end = 7750;
const int resolution = 10; // 100Hz*10 = 1kHz
const int fsize = (end-start)/resolution;
uint32_t val[fsize]; // size: 550
uint8_t bval[2*fsize+5];

    int idx=0;
    for(int i=start; i<end; i+=resolution)
    {
        val[idx] = 0;
        for(int j=0; j<resolution; j++)
			if(val[idx] < wfsamp[i+j]) val[idx] = wfsamp[i+j];
        idx++;
    }

    // val has 550 values, resolution 1kHz

    // convert to byte string
    idx = 5;
    for(int i=0; i<550; i++)
    {
        bval[idx++] = val[i] >> 8;
        bval[idx++] = val[i] & 0xff;
    }

	// 1st/2nd byte at 10489,475
	// next at 10489,476 ...

    // insert header
    uint32_t extDataID = 0x7743fa9f;
    bval[0] = extDataID >> 24;
	bval[1] = extDataID >> 16;
	bval[2] = extDataID >> 8;
	bval[3] = extDataID & 0xff;
    bval[4] = 1;    // ID for NB spectrum data
    
    // and send via UDP to HSmodem
    sendUDP(BEACON_UDP_IP,BEACON_UDP_PORT,bval,idx);
}

// WB Transponder
// gets the FFT result for an external application
// wfsamp ... FFT data starting at 10491.500 until 10499,500 (8MHz) Resolution 5kHz
// len ... 1600 values (8MHz / 1600 = 5kHz)

// we reduce this to 30kHz per value (every 6th value), which gives 266 values
// 10491.500 + 266*0.03 = 10499,480
void sendExtWB(uint16_t *wfsamp, int len)
{
#ifndef AMSATBEACON
    return;
#endif
    
const int start = 0;
const int end = 1600;
const int resolution = 6; // 5kHz * 6 = 30kHz
const int fsize = (end-start)/resolution;
uint32_t val[fsize]; // size: 266
uint8_t bval[2*fsize+5];

    int idx=0;
    for(int i=start; i<end; i+=resolution)
    {
        val[idx] = 0;
        for(int j=0; j<resolution; j++)
			if(val[idx] < wfsamp[i+j]) val[idx] = wfsamp[i+j];
        idx++;
    }

    // val has fsize values, resolution 1kHz

    // convert to byte string
    idx = 5;
    for(int i=0; i<fsize; i++)
    {
        bval[idx++] = val[i] >> 8;
        bval[idx++] = val[i] & 0xff;
    }

	// 1st/2nd byte at 10489,475
	// next at 10489,476 ...

    // insert header
    uint32_t extDataID = 0x7743fa9f;
    bval[0] = extDataID >> 24;
	bval[1] = extDataID >> 16;
	bval[2] = extDataID >> 8;
	bval[3] = extDataID & 0xff;
    bval[4] = 3;    // ID for WB spectrum data
    
    // and send via UDP to HSmodem
    sendUDP(BEACON_UDP_IP,BEACON_UDP_PORT,bval,idx);
}
