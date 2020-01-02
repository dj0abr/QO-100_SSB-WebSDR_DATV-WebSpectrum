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
* playSDRweb.h ... configuration options for playSDRweb
*  
* 
*/
#include <stdint.h>

//#define SOUNDLOCAL    // activate to send the sound to the local sound card (and not to the browser)

// ====================================================
// global definitions describing the LNB/Hardware/Mixer
// ====================================================
// enter the LNB crystal frequency and choose the multiplier for a 25 or 27 MHz LNB
#define LNB_CRYSTAL		24	    // enter the Crystal or external ref. frequency of your LNB in MHz
#define LNB_MULTIPLIER	390000	// enter the multiplier*1000 wich is 390 for a 25 MHz LNB
//#define LNB_MULTIPLIER	361112	// enter the multiplier*1000 wich is 361,112 for a 27 MHz LNB 

// global calculations, DO NOT change !
// LO of the LNB's internal mixer in kHz
#define LNB_LO		(LNB_CRYSTAL * LNB_MULTIPLIER)

// =====================================================
// definitions für the QO-100 wide band transponder mode
// =====================================================
// "WIDEBAND" is defined in the Makefile

#ifdef WIDEBAND
    // SDRplay must be used in wideband mode (RTLsdr is too slow)
	#ifndef	SDR_PLAY
		#define SDR_PLAY				
	#endif
    
    // definitions which may be modified to adapt the monitor to your needs
    // ====================================================================

    // this port must be opened in the router in order to use this software from the internet
    // (the usual web port 80 must also be open)
    #define WEBSOCK_PORT    8090
    
    // RX frequency of the left margin of the WF/spectrum picture in kHz
    #define DISPLAYED_FREQUENCY_KHZ  10491500  
    
    // fixed values DO NOT change !
    // ============================
    
	// 10 MHz sample rate, the maximum of the SDRplay hardware
    #define SDR_SAMPLE_RATE 10000000    
    
    // rate = 8MHz, which is the max bandwidth of the RSP1a
    // is a bit small, the left&right margins will be already clipped by the bandwidth,
    // but it is the maximum the RSP can handle.
    #define WF_RANGE_HZ 8000000     
    
    // width of the waterfall in pixels (must match the graphic width in the HTML file)
    #define WF_WIDTH    1600            
    
    // calculated values DO NOT change !
    // =================================
    
    #define _TUNED_FREQUENCY    (((DISPLAYED_FREQUENCY_KHZ + (WF_RANGE_HZ / 2)) - LNB_LO) * 1000)

#else  

    // =======================================================
    // definitions für the QO-100 narrow band transponder mode
    // =======================================================

    // definitions which may be modified to adapt the monitor to your needs
    // ====================================================================
    
    // RX frequency of the left margin of the waterfall/spectrum picture in kHz
    // the complete picture goes from DISPLAYED_FREQUENCY_KHZ to DISPLAYED_FREQUENCY_KHZ+(WF_RANGE_HZ/1000)
    #define DISPLAYED_FREQUENCY_KHZ  10489525  
    
    // width of the waterfall/spectrum display in Hz
    // if modified: check the SSB and AUDIO rate assignments below, and
    // check if the default rates are integer parts (see below: SSB_RATE and AUDIO_RATE)
    #define WF_RANGE_HZ 300000 
    
    // set the multiplier to 2,4,8,16 or 32 so that
    // for SDRplay:
    //    WF_RANGE_HZ * 2 * SR_MULTIPLIER is a value between 2M and 10M
    // for RTLsdr:
    //    WF_RANGE_HZ * 2 * SR_MULTIPLIER is a value between 900k and 2.4M
    #define SR_MULTIPLIER   4
    
    #ifdef SDR_PLAY
        #if (((WF_RANGE_HZ * 2 * SR_MULTIPLIER)<2000000) || ((WF_RANGE_HZ * 2 * SR_MULTIPLIER)>10000000))
            #warning SDRplay: CHOOSE OTHER SR_MULTIPLIER
        #endif
    #else
        #if (((WF_RANGE_HZ * 2 * SR_MULTIPLIER)<900000) || ((WF_RANGE_HZ * 2 * SR_MULTIPLIER)>2400000))
            #warning RTLSDR: CHOOSE OTHER SR_MULTIPLIER
        #endif
    #endif
    
    #if (DISPLAYED_FREQUENCY_KHZ > 10489535)
        #warning DISPLAYED_FREQUENCY_KHZ too high, there must be a marging of at least 15kHz to the beacon
    #endif
    
    // add a correction value to the SDR tuner frequency to compensate frequency
    // errors of the SDR hardware's crystal oscillator ( 0 = no correction)
    // this value is "ppm". 
    // usual values for the RTLsdr between -100 and +100 ppm
    // !!! this setting depends on the individual SDR hardware. First set it to 0, then enter a value !!!
    // small differences compensate with the AUTO-LOCK function in the browser
    // use the CW beacon for orientation
    // (not required for the SDRplay)
    #define RTL_TUNER_CORRECTION        0
    
    // this port must be opened in the router in order to use this software from the internet
    // (the usual web port 80 must also be open)
    #define WEBSOCK_PORT    8091
    
    
    // fixed values DO NOT change !
    // ============================

    // default SDR tuner frequency in Hz, which must be the left margin of the spectrum
    // default: 10489.55 (CW Beacon) minus 25 KHz for a little left margin:
    // (10489525 - 9750000)*1000 = 739525000 Hz
    #define _TUNED_FREQUENCY    ((DISPLAYED_FREQUENCY_KHZ - LNB_LO) * 1000)       

    // Frequency of the CW Beacon which is used for automatic freq correction, in Hz
    #define CW_BEACON   10489550000

	// SDR receiver's capture rate
	// we use twice the bandwidth, so the FFT will return the requested bandwidth in the lower half of the FFT data
	// the upper half (which contains the same bandwidth of below the tuner frequency) is not used to get a cleaner display
	// (the WB monitor uses both halfs)
    #define NB_SAMPLE_RATE     (WF_RANGE_HZ * 2)
	#define SDR_SAMPLE_RATE    (NB_SAMPLE_RATE * SR_MULTIPLIER)
	
	// we need one FFT value every 10Hz for the lower waterfall and the SSB demodulator
	// the Waterfall speed will be NB_RESOLUTION lines/s
	// if this is modified, the lower WF resolution must be also modified and the SSB demodulator may sound different
    #define NB_RESOLUTION   10  
    
    // width of the waterfall in pixels (must match the graphic width in the HTML file)
    #define WF_WIDTH    1500            
    
    
    // calculated values DO NOT change !
    // =================================
    
    // number of input and output data of the FFT
    // we will only use the lower half of the output data
    // i.e.: 600.000 / 10 = 60.000 samples input and 60.000 bins FFT output, where 30.000 are used
    // these 30.000 bins show the spectrum of 300kHz with a resolution of 10 Hz
    #define NB_FFT_LENGTH   (NB_SAMPLE_RATE / NB_RESOLUTION)
    
    // we have more FFT data as we can show in the display (display resolution: WF_WIDTH)
    // this value is the oversampling and MUST be an integer number !!!
    // i.e.: NB_FFT_LENGTH/2 = 30.000 divided by WF_WIDTH=1500 is 20
    // so we will use 20 FFT values for one screen pixel
    #define NB_OVERSAMPLING ((NB_FFT_LENGTH/2) / WF_WIDTH)
    
    // Hz per screen pixel
    // NB_OVERSAMPLING shows the FFT values per pixel, but one 
    // FFT value is for NB_RESOLUTION Hz, so we multiply them
    #define NB_HZ_PER_PIXEL (NB_OVERSAMPLING * NB_RESOLUTION)

    // the SSB rate must be an integer part of NB_SAMPLE_RATE and must be <= 48k
    // AUDIO rate must be an integer part of SSB rate
    // recalculate and add values for non-standard ranges WF_RANGE_HZ

    #if WF_RANGE_HZ == 150000
        #define SSB_RATE 37500
        #define AUDIO_RATE 7500
    #else
        #define SSB_RATE 40000  // use default rate if possible
        #define AUDIO_RATE 8000
    #endif

#endif


// for the RTLSDR only !
// samples per callback
#define SAMPLES_PER_PASS    512*8 // must be a multiple of 512


extern int hwtype;
extern int samplesPerPacket;
extern int TUNED_FREQUENCY;
