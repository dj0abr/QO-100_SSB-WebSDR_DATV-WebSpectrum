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
* hilbert90.c ... band pass to shift by +45 and -45 deg
* this gives 90 degrees, but I and Q have the same delay
* 
* the tables are for +45 degrees
* for -45 deg use the same table, but reverse
* 
*/

#include <stdio.h>
#include "qo100websdr.h"
#include "websocket/websocketserver.h"

/*
* designed with Iowa Hills Hilbert Filter Designer
* Parameters:
* Phase Added 
* Smapling Freq: 8000 (does not matter, is just for fun)
* Fc=1,88kHz (0,44)
* BW=3.6 kHz (0,731)
* Taps: 24
* Window: Kaiser
* Raised Cosine: 0,84
*/

#define tabsHILB          24  // tabs of hilbert band pass

// bandwidth 3,6 kHz
double FIR_Hilbert_45[tabsHILB] = {
 -0.00260492718417866	,
-0.000435279616289449	,
-0.0121601603520926	,
0.000384350455331288	,
-0.0168559726027298	,
-0.0160874848133516	,
0.0132043169633668	,
-0.0551943653880796	,
0.0768106536919752	,
-0.0378433829340764	,
0.111770777916182	,
0.702784136868646	,
0.0664326602842022	,
-0.384717540796358	,
-0.0192821666915141	,
-0.0990236979721239	,
-0.0626057184936948	,
-0.00780901124631701	,
-0.0446847081582393	,
0.00266972379689263	,
-0.0122302291689453	,
-0.00271811787696641	,
0.00100587186558935	,
-0.00177030669016904	
};

double BandPass45deg(double sample, int client)
{
static double circular_buffer[MAX_CLIENTS][tabsHILB];
double *pcoeff = FIR_Hilbert_45;
static int wr_idx[MAX_CLIENTS];
double y;

    // write value to buffer
    circular_buffer[client][wr_idx[client]] = sample;
    
    // increment write index
    wr_idx[client]++;
    wr_idx[client] %= tabsHILB;
    
    // calculate new value
    y = 0;
    int idx = wr_idx[client];
    for(int i = 0; i < tabsHILB; i++)
    {
        y += (*pcoeff++ * circular_buffer[client][idx++]);
        if(idx >= tabsHILB) idx=0;
    }

    return y;
}

// -45 deg uses the same coeff table, but in reverse order
double BandPassm45deg(double sample, int client)
{
static double circular_buffer[MAX_CLIENTS][tabsHILB];
double *pcoeff = FIR_Hilbert_45 + tabsHILB - 1; // goto the last value in the table
static int wr_idx[MAX_CLIENTS];
double y;

    // write value to buffer
    circular_buffer[client][wr_idx[client]] = sample;
    
    // increment write index
    wr_idx[client]++;
    wr_idx[client] %= tabsHILB;
    
    // calculate new value
    y = 0;
    int idx = wr_idx[client];
    for(int i = 0; i < tabsHILB; i++)
    {
        y += (*pcoeff-- * circular_buffer[client][idx++]);
        if(idx >= tabsHILB) idx=0;
    }

    return y;
}
