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
* fir_table_calc.c ... calculate coefficients for a FIR low pass filter
 * ====================================================================
 * 
 * sampleRate ... sample rate from the source (i.e. soundcard, SDR...)
 * freq_minus3db ... frequency for an attenuation of 3dB
 * pcofs ... an empty double buffer with the following length
 * length ... number of coeffs to be calculated (must be an odd number !)
 * 
 * Hints for choosing the length:
 * - as short as possible for low CPU load
 * - 201 looks to be the minimum usable length
 * - 301 good medium value
 * - 401 sharp good filter
 * 
 * Hints for choosing the freq_minus3db
 * - can usually be 10% less than the needed edge depending on the filter length
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "fir_table_calc.h"

double PI2 = 2.0 * M_PI;    // 2 * pi

void createLowPassFIRfilter(double sampleRate, double freq_minus3db, double *pcoeffs, int length) 
{
    if((length & 1) == 0)
    {
        // terminate program if length is wrong
        printf("createLowPassFIRfiler: length MUST be an odd number !\n");
        exit(0);
    }
 
    // calculate the coeffs
    double freq = freq_minus3db / sampleRate;
    int mid = floor(length / 2);
    double sum = 0;
    double val = 0;

    for (int i = 0; i < length; i++) 
    {
        if (i == mid) 
            val = PI2 * freq;
        else 
        {
            val = (sin(PI2 * freq * (double)(i - mid)) / (double)(i - mid)) * (0.54 - 0.46 * cos(PI2 * i / (double)(length - 1)));
        }
        sum += val;
        pcoeffs[i] = val;
    }

    for (int i = 0; i < length; i++) 
        pcoeffs[i] /= sum;
}
