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
// enter the LNB crystal frequency or external LO frequency
#define DEFAULT_LNB_CRYSTAL		24000000

// enter the multiplier*1000 which is 390 for a 25 MHz LNB
// or 361,111 for a 27 MHz LNB 
#define DEFAULT_LNB_MULTIPLIER	390000
//#define DEFAULT_LNB_MULTIPLIER	361111

// if a downmixer is used, enter the mixer's output frequency here (only MHz, i.e.: 439)
// if no downmixer is used, enter 0
#define DEFAULT_DOWNMIXER_OUTQRG       0       

// =====================================================
// definitions für the QO-100 wide band transponder mode
// =====================================================
// "WIDEBAND" is defined in the Makefile

#ifdef WIDEBAND
    // this port must be opened in the router in order to use this software from the internet
    // (the usual web port 80 must also be open)
    #define DEFAULT_WEBSOCK_PORT    8091
    
    #ifdef	SDR_PLAY
        // RX frequency of the left margin of the WF/spectrum picture in kHz
        #define LEFT_MARGIN_QRG_KHZ  10491500  
        
        // 10 MHz sample rate, the maximum of the SDRplay hardware
        #define SDR_SAMPLE_RATE 10000000    
        
        // rate = 8MHz, which is the max bandwidth of the RSP1a
        // is a bit small, the left&right margins will be already clipped by the bandwidth,
        // but it is the maximum the RSP can handle.
        #define WF_RANGE_HZ 8000000     
        
        // width of the waterfall in pixels (must match the graphic width in the HTML file)
        #define WF_WIDTH    1600 
	#endif
	
    #ifdef	PLUTO
        // RX frequency of the left margin of the WF/spectrum picture in kHz
        #define LEFT_MARGIN_QRG_KHZ  10491500  
        
        // 14 MHz sample rate, the Pluto has already drop outs at this rate
        // but this does not matter for a Spectrum/WF display
        #define SDR_SAMPLE_RATE 16000000    
        
        // BW= 8 MHz
        #define WF_RANGE_HZ 8000000     
        
        // width of the waterfall in pixels (must match the graphic width in the HTML file)
        #define WF_WIDTH    1600 
	#endif
#else  

    // =======================================================
    // definitions für the QO-100 narrow band transponder mode
    // =======================================================
    
    // this port must be opened in the router in order to use this software from the internet
    // (the usual web port 80 must also be open)
    #define DEFAULT_WEBSOCK_PORT    8091
    
    // we always capture the complete NB transponder over its total size
    // beginning at 10489,250 MHz with a range of 900kHz to 10490,150 MHz
    // the selection of a part of these data are done in the browser
    
    // RX frequency of the left margin of the waterfall/spectrum in kHz
    // the complete samples go from LEFT_MARGIN_QRG_KHZ to LEFT_MARGIN_QRG_KHZ+(WF_RANGE_HZ/1000)
    #define LEFT_MARGIN_QRG_KHZ  10489250  
    
    // width of the waterfall/spectrum in Hz
    // if modified: check the SSB and AUDIO rate assignments below, and
    // check if the default rates are integer parts (see below: SSB_RATE and AUDIO_RATE)
    #define WF_RANGE_HZ 900000 
    
    // Frequency of the CW Beacon which is used for automatic freq correction, in Hz
    #define CW_BEACON   10489500000

   	// we need one FFT value every 10Hz for the lower waterfall
	// the Waterfall speed will be NB_RESOLUTION lines/s
	// if this is modified, the lower WF resolution must be also modified
    #define NB_RESOLUTION   10  
    
    // width of the waterfall in pixels (must match the graphic width in the HTML file)
    #define WF_WIDTH    1500            

    // set the multiplier to 1(only RTLsdr),2,4,8,16 or 32 so that
    // for SDRplay:
    //    WF_RANGE_HZ * 2 * SR_MULTIPLIER is a value between 2M and 10M
    // for RTLsdr:
    //    WF_RANGE_HZ * 2 * SR_MULTIPLIER is a value between 900k and 2.4M
    #ifdef SDR_PLAY
        #define SR_MULTIPLIER   2   // default sample rate: 0.9 * 2 * 2 = 3.6M
    #elif PLUTO
        #define SR_MULTIPLIER   2   // default sample rate: 0.9 * 2 * 2 = 3.6M
    #else
        #define SR_MULTIPLIER   1   // default sample rate: 0.9 * 2 * 1 = 1.8M
    #endif
    
    // check if multiplier are within range
    #ifdef SDR_PLAY
        #if (((WF_RANGE_HZ * 2 * SR_MULTIPLIER)<2000000) || ((WF_RANGE_HZ * 2 * SR_MULTIPLIER)>10000000))
            #warning SDRplay: CHOOSE OTHER SR_MULTIPLIER
        #endif
    #elif PLUTO
        #if (((WF_RANGE_HZ * 2 * SR_MULTIPLIER)<2000000) || ((WF_RANGE_HZ * 2 * SR_MULTIPLIER)>10000000))
            #warning PLUTO: CHOOSE OTHER SR_MULTIPLIER
        #endif
    #else
        #if (((WF_RANGE_HZ * 2 * SR_MULTIPLIER)<900000) || ((WF_RANGE_HZ * 2 * SR_MULTIPLIER)>2400000))
            #warning RTLSDR: CHOOSE OTHER SR_MULTIPLIER
        #endif
    #endif

    // calculated values
    // =================

	// SDR receiver's capture rate
	// we use twice the bandwidth, so the FFT will return the requested bandwidth in the lower half of the FFT data
	// the upper half (which contains the same bandwidth of below the tuner frequency) is not used to get a cleaner display
	// (the WB monitor uses both halfs)
	// we get data from the SDR with a rate of NB_SAMPLE_RATE
    #define NB_SAMPLE_RATE     (WF_RANGE_HZ * 2)                // 900000*2= 1.800.000
    
    // SDR_SAMPLE_RATE is the SDRs internal sample rate
    // the SDRplay divides it in the proprietary driver, while
    // the RTLsdr divides it in rtlsdr.c
	#define SDR_SAMPLE_RATE    (NB_SAMPLE_RATE * SR_MULTIPLIER)
    
    // number of input and output data of the FFT
    // i.e.: 1.800.000 / 10 = 180.000 samples input and 180.000 bins FFT output, where 90.000 are used
    // these 90.000 bins show the spectrum of 900kHz with a resolution of 10 Hz
    #define NB_FFT_LENGTH   (NB_SAMPLE_RATE / NB_RESOLUTION)    // 1.800.000/10 = 180.000

    // the FFT delivers NB_FFT_LENGTH/2 values (i.e.: 90.000 values with a resolution of 10 Hz/value)
    // we want to be able to display a lowest range of 150kHz (30.000 values) within 1500 pixel
    // this is 200Hz/Pixel
    // the FFT gives us 10 Hz value, so we only need every 10th value, this is the oversampling
    #define NB_OVERSAMPLING (15000 / WF_WIDTH)  // = 10
    
    // Hz per screen pixel
    // NB_OVERSAMPLING shows the FFT values per pixel, but one 
    // FFT value is for NB_RESOLUTION Hz, so we multiply them
    #define NB_HZ_PER_PIXEL (NB_OVERSAMPLING * NB_RESOLUTION)

    // the AUDIO rate must be an integer part of NB_SAMPLE_RATE and must be <= 48k
    // recalculate and add values for non-standard ranges WF_RANGE_HZ

    #if WF_RANGE_HZ == 150000
        #define AUDIO_RATE 7500
    #else
        #define AUDIO_RATE 8000 // use default rate if possible
    #endif

#endif

// for the RTLSDR only !
// samples per callback
#define SAMPLES_PER_PASS    512*8 // must be a multiple of 512

extern int hwtype;
extern int samplesPerPacket;
extern int TUNED_FREQUENCY;
extern int stopped;
