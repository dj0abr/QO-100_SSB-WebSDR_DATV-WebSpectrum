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
* 
* rtlsdr.c ... driver for the RTLSDR stick
* 
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <rtl-sdr.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include "qo100websdr.h"
#include "fifo.h"
#include "ssbfft.h"

void *rtldevproc(void *pdata);
void *rtlproc(void *pdata);

static rtlsdr_dev_t *dev;

int init_rtlsdr()
{
    int rtldevicenumber = 0;
    
    // check if an RTL device is available
    int devices = rtlsdr_get_device_count();
    printf("found %d RTLSDR devices\n",devices);
    if(devices == 0) return 0;
    if(devices > 1)
    {
        printf("this program does work with the FIRST rtl device only !\n");
    }
    printf("Device: %s\n\n",rtlsdr_get_device_name(rtldevicenumber));

    // open the first device
    int retval = rtlsdr_open(&dev, rtldevicenumber);
    printf("open returned: %d\n",retval);
    
    // configure the device
    int samprate = 1200000;
    retval = rtlsdr_set_sample_rate(dev, samprate);
    if(retval != 0) printf("Sample rate %d set= %d\n",samprate,retval);
    
    retval = rtlsdr_set_center_freq(dev, TUNED_FREQUENCY);
    if(retval != 0) printf("freqset= %d\n",retval);
    
    // gain mode: 0=auto, 1=manual
    retval = rtlsdr_set_tuner_gain_mode(dev, 1);
    if(retval != 0) printf("Gain mode set= %d\n",retval);
    
    retval = rtlsdr_set_agc_mode(dev, 1);
    if(retval != 0) printf("Set agc= %d\n",retval);
    
    // Find the maximum gain available
    int gains[100];
    int numgains = rtlsdr_get_tuner_gains(dev, gains);
    int gain = gains[numgains-1];
    rtlsdr_set_tuner_gain(dev, gain);
	printf("Setting gain to: %.2f\n", gain/10.0);

    retval = rtlsdr_reset_buffer(dev);
    if(retval != 0) printf("rtlsdr_reset_buffer= %d\n",retval);
    
    printf("=== [R82XX] PLL locked, OK ===\n");
    
    // start capture process
    // this process ONLY reads data from the RTLSDR device
    // into a buffer
    pthread_t rtldev_pid = 0;
    int ret = pthread_create(&rtldev_pid,NULL,rtldevproc, NULL);
    if(ret)
    {
        printf("rtldev_pid: rtldevproc NOT started\n\r");
        return 0;
    }
    
    // this thread processes received samples
    pthread_t rtlproc_pid = 0;
    ret = pthread_create(&rtlproc_pid,NULL,rtlproc, NULL);
    if(ret)
    {
        printf("rtlproc_pid: rtlproc NOT started\n\r");
        return 0;
    }
    
    return 1;
}

void rtlsetTunedQrgOffset(unsigned int hz)
{
    unsigned int qrg = TUNED_FREQUENCY - hz;
    int retval = rtlsdr_set_center_freq(dev, qrg);
    if(retval != 0) printf("freqset= %d\n",retval);
    printf("rtl rf : %d\n",qrg);
}

void rtlsdrCallback(unsigned char *buf, uint32_t len, void *ctx)
{
    if (len > SAMPLES_PER_PASS) 
    {
        printf("error: data len too long %d\n",len);
        return;
    }
    write_pipe(FIFO_RTL, buf, len);
}

void *rtldevproc(void *pdata)
{
    pthread_detach(pthread_self());
    
    // this function blocks forever, never comes back
    // rtlsdrCallback is called with new data
    // the callback function is called every SAMPLES_PER_PASS received bytes (=SAMPLES_PER_PASS/2 IQ Samples)
    // this number MUST be a multiple of 512
    rtlsdr_read_async(dev, rtlsdrCallback, NULL, 0, SAMPLES_PER_PASS);
    
    // never comes here
    pthread_exit(NULL);
}

int maxn = 0;
// processing of received data
void *rtlproc(void *pdata)
{
unsigned char buf[SAMPLES_PER_PASS];
short ibuf[SAMPLES_PER_PASS/2];
short qbuf[SAMPLES_PER_PASS/2];
int ret;

    pthread_detach(pthread_self());
    
    // process the data received by rtlsdrCallback
    while(1)
    {
        ret = read_pipe_wait(FIFO_RTL, buf, SAMPLES_PER_PASS);
        if(ret)
        {
            int dstlen = 0;
            // incr by 4: 2 because MSB and LSB, and 2 for decimation from 1M2 to 600k
            for(int i=0; i<SAMPLES_PER_PASS; i+=4)
            {
                double fi = buf[i];
                fi -= 127.4;
                fi *= 256;
                ibuf[dstlen] = (short)fi;
                
                double fq = buf[i+1];
                fq -= 127.4;
                fq *= 256;
                qbuf[dstlen] = (short)fq;
                
                dstlen++;
            }
            
            //sample_processing(ibuf, qbuf, dstlen);
            fssb_sample_processing(ibuf, qbuf, dstlen);
        }
    }
       
    // never comes here
    pthread_exit(NULL);
}
