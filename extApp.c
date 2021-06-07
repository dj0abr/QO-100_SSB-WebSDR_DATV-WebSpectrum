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

char beacon_udp_ip[20] = {0}; // IP address where the HSmodem beacon is running

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
    sendUDP(beacon_udp_ip,BEACON_UDP_PORT,bval,idx);
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
uint32_t val[fsize+1]; // size: 266
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
    sendUDP(beacon_udp_ip,BEACON_UDP_PORT,bval,idx);
}

// ============= read Beacon Config file ===================

char* cleanStr(char* s)
{
    if (s[0] > ' ')
    {
        // remove trailing crlf
        for (size_t j = 0; j < strlen(s); j++)
            if (s[j] == '\n' || s[j] == '\r') s[j] = 0;
        return s;    // nothing to do
    }

    for (size_t i = 0; i < strlen(s); i++)
    {
        if (s[i] >= '0')
        {
            // i is on first character
            memmove(s, s + i, strlen(s) - i);
            s[strlen(s) - i] = 0;
            // remove trailing crlf
            for (size_t j = 0; j < strlen(s); j++)
                if (s[j] == 'n' || s[j] == '\r') s[j] = 0;
            return s;
        }
    }
    return NULL;   // no text found in string
}

char* getword(char* s, int idx)
{
    if (idx == 0)
    {
        for (size_t i = 0; i < strlen(s); i++)
        {
            if (s[i] < '0')
            {
                s[i] = 0;
                return s;
            }
        }
        return NULL;
    }

    for (size_t j = 0; j < strlen(s); j++)
    {
        if (s[j] > ' ')
        {
            char* start = s + j;
            for (size_t k = 0; k < strlen(start); k++)
            {
                if (start[k] == ' ' || start[k] == '\r' || start[k] == '\n')
                {
                    start[k] = 0;
                    return start;
                }
            }
            return start;
        }
    }

    return NULL;
}

// read the value of an element from the config file
// Format:
// # ... comment
// ElementName-space-ElementValue
// the returned value is a static string and must be copied somewhere else
// before this function can be called again
char* getConfigElement(char* elemname)
{
    static char s[501];
    int found = 0;
    char fn[1024];
    
    if(strlen(CONFIGFILE) > 512)
    {
        printf("config file path+name too long: %s\n",CONFIGFILE);
        exit(0);
    }
    strcpy(fn,CONFIGFILE);
    
    if(fn[0] == '~')
    {
        struct passwd *pw = getpwuid(getuid());
        const char *homedir = pw->pw_dir;
        sprintf(fn,"%s%s",homedir,CONFIGFILE+1);
    }

    printf("read Configuration file %s\n",fn);
    FILE *fr = fopen(fn,"rb");
    if(!fr) 
    {
        printf("!!! Configuration file %s not found !!!\n",fn);
        exit(0);
    }

    while (1)
    {
        if (fgets(s, 500, fr) == NULL) break;
        // remove leading SPC or TAB
        if (cleanStr(s) == 0) continue;
        // check if its a comment
        if (s[0] == '#') continue;
        // get word on index
        char* p = getword(s, 0);
        if (!p) break;
        if (strcmp(p, elemname) == 0)
        {
            char val[500];
            if (*(s + strlen(p) + 1) == 0) continue;
            p = getword(s + strlen(p) + 1, 1);
            if (!p) break;
            // replace , with .
            char* pkomma = strchr(p, ',');
            if (pkomma) *pkomma = '.';
            strcpy(val, p);
            strcpy(s, val);
            found = 1;
            break;
        }

    }

    fclose(fr);
    printf("Element: <%s> ", elemname);
    if (found == 0) 
    {
        printf("not found\n");
        return NULL;
    }
    printf(":<%s>\n",s);
    return s;
}

void readAMSConfig()
{
#ifndef AMSATBEACON
    return;
#endif

    // set defaults from #define
    memset(beacon_udp_ip,0,sizeof(beacon_udp_ip));
    strncpy(beacon_udp_ip, BEACON_UDP_IPADDR, sizeof(beacon_udp_ip)-1);

    // read IP address of the HSmodem beacon
    char *hp = getConfigElement("UDP_IPADDRESS");
    if(hp) strncpy(beacon_udp_ip, hp, sizeof(beacon_udp_ip)-1);

    // read serial number of pluto (for WB only, because NB is handled by the NB pluto-driver)
    memset(pluto_serialnumber,0,sizeof(pluto_serialnumber));
    hp = getConfigElement("PLUTO_WIDEBAND");
    if(hp) strncpy(pluto_serialnumber,hp, sizeof(pluto_serialnumber)-1);
}
