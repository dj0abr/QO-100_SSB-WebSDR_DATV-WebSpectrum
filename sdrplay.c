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
* sdrplay.c ... handles the SDRplay hardware
* 
*/

double lastsdrqrg = 0;


#ifdef SDR_PLAY

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include "qo100websdr.h"
#include "sdrplay.h"
#include "ssbfft.h"
#include "wb_fft.h"
#include "setup.h"
#include "sdrplay_api.h"  // the SDRplay driver must be installed !

void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
sdrplay_api_EventParamsT *params, void *cbContext);
void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext);
void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext);

int device = 0;     // device number. If we have one SDRplay device connected: device=0
int devModel = 1;
float ppmOffset = 0.0;
int antenna = 0; // 0 = Ant A, 1 = Ant B, 2 = HiZ
sdrplay_api_Rsp2_AntennaSelectT ant = sdrplay_api_Rsp2_ANTENNA_A;
int refClk = 0;
int notchEnable = 0;
int biasT = 0;
int gainR = 50;

int ifkHz = 0;      // 0 is used in this software
int rspLNA = 0;
void *cbContext = NULL;
int agcControl = 1;
long setPoint = -30;
sdrplay_api_DeviceT *chosenDevice = NULL;
sdrplay_api_RxChannelParamsT * chParams;
sdrplay_api_DeviceT devs[6];
unsigned int ndev;
float ver = 0.0;
sdrplay_api_ErrT err;
sdrplay_api_DeviceParamsT *deviceParams = NULL;
sdrplay_api_CallbackFnsT cbFns;
int reqTuner = 0;
int master_slave = 0;
unsigned int chosenIdx = 0;

/*
 * init the SDRplay hardware
 * when this function suceeds, then
 * streamCallback
 * will be called delivering new samples
 */
int init_SDRplay()
{
int i;

    printf("Initialize SDRplay hardware\n");

	if ((err = sdrplay_api_Open()) != sdrplay_api_Success)
	{
		printf("sdrplay_api_Open failed %s\n", sdrplay_api_GetErrorString(err));
        return 0;
	}
    // Check API versions match
    if ((err = sdrplay_api_ApiVersion(&ver)) != sdrplay_api_Success)
    {
        printf("sdrplay_api_ApiVersion failed %s\n", sdrplay_api_GetErrorString(err));
    }
    if (ver != SDRPLAY_API_VERSION)
    {
        printf("API version don't match (local=%.2f dll=%.2f)\n", SDRPLAY_API_VERSION, ver);
        sdrplay_api_Close();
        return 0;
    }

    sdrplay_api_LockDeviceApi();    // Lock API while device selection is performed

    // Fetch list of available devices
    if ((err = sdrplay_api_GetDevices(devs, &ndev, sizeof(devs) / sizeof(sdrplay_api_DeviceT))) != sdrplay_api_Success)
    {
        printf("sdrplay_api_GetDevices failed %s\n", sdrplay_api_GetErrorString(err));
        sdrplay_api_UnlockDeviceApi();
        sdrplay_api_Close();
        return 0;
    }

    printf("MaxDevs=%ld NumDevs=%d\n", sizeof(devs) / sizeof(sdrplay_api_DeviceT), ndev);

    if (ndev == 0)
    {
        printf("no SDRplay devices found\n");
        sdrplay_api_UnlockDeviceApi();
        sdrplay_api_Close();
        return 0;
    }

    if (ndev > 0)
    {
        for (i = 0; i < (int) ndev; i++)
        {
            if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
                printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x rspDuoMode=0x%.2x\n", i,
                    devs[i].SerNo, devs[i].hwVer, devs[i].tuner, devs[i].rspDuoMode);
            else
                printf("Dev%d: SerNo=%s hwVer=%d tuner=0x%.2x\n", i, devs[i].SerNo,
                    devs[i].hwVer, devs[i].tuner);
        }
        // Choose device
        if ((reqTuner == 1) || (master_slave == 1))	// requires RSPduo
        {
            // Pick first RSPduo
            for (i = 0; i < (int) ndev; i++)
            {
                if (devs[i].hwVer == SDRPLAY_RSPduo_ID)
                {
                    chosenIdx = i;
                    break;
                }
            }
        }
        else
        {
            // Pick first device of any type
            for (i = 0; i < (int) ndev; i++)
            {
                chosenIdx = i;
                break;
            }
        }
        if (i == ndev)
        {
            printf("Couldn't find a suitable device to open - exiting\n");
            sdrplay_api_UnlockDeviceApi();
            sdrplay_api_Close();
            return 0;

        }

        printf("chosenDevice = %d\n", chosenIdx);

        chosenDevice = &devs[chosenIdx];
        // If chosen device is an RSPduo, assign additional fields
        if (chosenDevice->hwVer == SDRPLAY_RSPduo_ID)
        {
            // If master device is available, select device as master
            if (chosenDevice->rspDuoMode &sdrplay_api_RspDuoMode_Master)
            {
                    // Select tuner based on user input (or default to TunerA)
                chosenDevice->tuner = sdrplay_api_Tuner_A;
                if (reqTuner == 1)
                    chosenDevice->tuner = sdrplay_api_Tuner_B;
                // Set operating mode
                if (!master_slave)	// Single tuner mode
                {
                    chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
                    printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x\n", chosenIdx,
                        chosenDevice->rspDuoMode, chosenDevice->tuner);
                }
                else
                {
                    chosenDevice->rspDuoMode = sdrplay_api_RspDuoMode_Master;
                    // Need to specify sample frequency in master/slave mode
                    chosenDevice->rspDuoSampleFreq = 6000000.0;
                    printf("Dev%d: selected rspDuoMode=0x%.2x tuner=0x%.2x rspDuoSampleFreq=%.1f\n",
                        chosenIdx, chosenDevice->rspDuoMode,
                        chosenDevice->tuner, chosenDevice->rspDuoSampleFreq);
                }
            }
            else	// Only slave device available
            {
                    // Shouldn't change any parameters for slave device
            }
        }

        // Select chosen device
        if ((err = sdrplay_api_SelectDevice(chosenDevice)) != sdrplay_api_Success)
        {
            printf("sdrplay_api_SelectDevice failed %s\n", sdrplay_api_GetErrorString(err));
            sdrplay_api_UnlockDeviceApi();
            sdrplay_api_Close();
            return 0;
        }

        sdrplay_api_UnlockDeviceApi();  // Unlock API now that device is selected

        // Retrieve device parameters so they can be changed if wanted
        if ((err = sdrplay_api_GetDeviceParams(chosenDevice->dev, &deviceParams)) != sdrplay_api_Success)
        {
            printf("sdrplay_api_GetDeviceParams failed %s\n", sdrplay_api_GetErrorString(err));
            sdrplay_api_Close();
            return 0;
        }
        // Check for NULL pointers before changing settings
        if (deviceParams == NULL)
        {
            printf("sdrplay_api_GetDeviceParams returned NULL deviceParams pointer\n");
            sdrplay_api_Close();
            return 0;
        }

        // Configure dev parameters
        if (deviceParams->devParams != NULL)
        {
            printf("======================== SR:%d\n",SDR_SAMPLE_RATE);
            deviceParams->devParams->fsFreq.fsHz = (double)SDR_SAMPLE_RATE;
            deviceParams->rxChannelA->ctrlParams.decimation.enable = 1;
            #ifndef WIDEBAND
            deviceParams->rxChannelA->ctrlParams.decimation.decimationFactor = SR_MULTIPLIER;
            #else
            deviceParams->rxChannelA->ctrlParams.decimation.decimationFactor = 1;
            #endif
        }

        // Configure tuner parameters (depends on selected Tuner which parameters to use)
        chParams = (chosenDevice->tuner == sdrplay_api_Tuner_B) ? deviceParams->rxChannelB :deviceParams->rxChannelA;
        if (chParams != NULL)
        {
            printf("set default QRG: %d\n",tuned_frequency);
            chParams->tunerParams.rfFreq.rfHz = tuned_frequency;
            #ifdef WIDEBAND
            chParams->tunerParams.bwType = sdrplay_api_BW_8_000;
            #else
            chParams->tunerParams.bwType = sdrplay_api_BW_1_536;
            #endif
            chParams->tunerParams.ifType = sdrplay_api_IF_Zero;
            chParams->tunerParams.gain.gRdB = 40;
            chParams->tunerParams.gain.LNAstate = 1;
            chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
        }
        else
        {
            printf("sdrplay_api_GetDeviceParams returned NULL chParams pointer\n");
            sdrplay_api_Close();
            return 0;
        }

        // Assign callback functions to be passed to sdrplay_api_Init()
        cbFns.StreamACbFn = StreamACallback;
        cbFns.StreamBCbFn = StreamBCallback;
        cbFns.EventCbFn = EventCallback;
        // Now we're ready to start by calling the initialisation function
        // This will configure the device and start streaming
        if ((err = sdrplay_api_Init(chosenDevice->dev, &cbFns, NULL)) != sdrplay_api_Success)
        {
            printf("sdrplay_api_Init failed %s\n", sdrplay_api_GetErrorString(err));
            sdrplay_api_ErrorInfoT *errInfo = sdrplay_api_GetLastError(NULL);
            if (errInfo != NULL)
                printf("Error in %s: %s(): line %d: %s\n", errInfo->file, errInfo->function, errInfo->line, errInfo->message);
            sdrplay_api_Close();
            return 0;
        }
    }
    
    return 1;
}

// clean up SDRplay device
void remove_SDRplay()
{
    printf("Close SDRplay hardware\n");

    sdrplay_api_ReleaseDevice(chosenDevice);
    sdrplay_api_Close();
}

void setTunedQrgOffset(int hz)
{
    if(lastsdrqrg == 0) lastsdrqrg = tuned_frequency;
    
    double off = (double)hz;
    lastsdrqrg = lastsdrqrg - off;
    printf("set tuner: new:%f offset:%f\n",lastsdrqrg,off);

    deviceParams->rxChannelA->tunerParams.rfFreq.rfHz = lastsdrqrg;

    if ((err = sdrplay_api_Update(chosenDevice->dev, 
                                chosenDevice->tuner,
                                sdrplay_api_Update_Tuner_Frf, 
                                sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
    {
        printf("sdrplay_api_Update reset_Qrg_SDRplay failed: %s\n",sdrplay_api_GetErrorString(err));
    }

    printf("set tuner : %.6f MHz\n",lastsdrqrg/1e6);
}

void reset_Qrg_SDRplay()
{
    sdrplay_api_ErrT err;

    lastsdrqrg = tuned_frequency;
    printf("re-tune: %.6f MHz\n",lastsdrqrg/1e6);

    deviceParams->rxChannelA->tunerParams.rfFreq.rfHz = lastsdrqrg;

    if ((err = sdrplay_api_Update(chosenDevice->dev, 
                                chosenDevice->tuner,
                                sdrplay_api_Update_Tuner_Frf, 
                                sdrplay_api_Update_Ext1_None)) != sdrplay_api_Success)
    {
        printf("sdrplay_api_Update reset_Qrg_SDRplay failed: %s\n",sdrplay_api_GetErrorString(err));
    }
}

/*
 * the SDRplay driver calls this function
 * numSamples ... number of samples included
 * xi and xq ... samples
 * */

void StreamACallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if (reset)
        printf("sdrplay_api_StreamACallback: numSamples=%d\n", numSamples);
    
#ifdef WIDEBAND
	wb_sample_processing(xi, xq, numSamples);
#else
    fssb_sample_processing(xi, xq, numSamples);
#endif // WIDEBAND
    
    return;
}

void StreamBCallback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, unsigned int reset, void *cbContext)
{
    if (reset)
        printf("sdrplay_api_StreamBCallback: numSamples=%d\n", numSamples);
    
    // Process stream callback data here
    
    return;
}

void EventCallback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
sdrplay_api_EventParamsT *params, void *cbContext)
{
}


#endif // SDR_PLAY
