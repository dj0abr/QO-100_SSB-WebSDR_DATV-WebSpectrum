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
* drawWF(int id, double *fdata, int cnt, int wpix, int hpix, int _leftqrg, int _rightqrg, int res, int _tunedQRG, char *fn);
* 
* id ... a unique number (used to create a temporary file name, see wf_univ.h)
* fdata ... a double array containing the FFT result
* cnt ... number of FFT results (or less)
* wpix ... width of the waterfall bitmap in pixels (equal or less then cnt)
* hpix ... height of the waterfall bitmap in pixels
* _leftqrg ... frequency offset of the left margin of the bitmap (usually 0, the first FFT value)
* _rightqrg ... frequency offset of the right margin of the bitmap (usually: FFTRESOLUTION in Hz * width in pixels)
* res ... FFT resolution in Hz per FFT value (= Hz per pixel)
* _tunedQRG ... frequency where the SDR receivers is tuned (used for displaying the frequncy in the title)
* fn ... path and filename of the bitmap
* 
* the bitmap is created as a temporary BMP file in the /tmp folder.
* after drawing this is copied to fn
* 
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gd.h>
#include "gdfontt.h"
#include "gdfonts.h"
#include "gdfontmb.h"
#include "gdfontl.h"
#include "gdfontg.h"
#include "wf_univ.h"
#include "color.h"
#include "websocketserver.h"
#include "setqrg.h"
#include "civ.h"

void drawWFimage(int id, gdImagePtr im, char *fn);
void drawFFTline(int id, gdImagePtr dst);
void drawQrgTitles(int id, gdImagePtr dst);
void scaleSamples(double *samples, int numSamples);

typedef struct {
    int dg_first;
    double *fftdata;    // data from fftcnt
    int fftcnt;         // number of values from fftcnt
    int pic_width;      // number of horizontal pixels of the resulting WF diagram
    int pic_height;     // number of vertial pixels of the resulting WF diagram
                        // set pic_height=1 for one pixel line only
    char filename[256]; // store the bitmap into this file
    int realqrg;        // qrg of the tuner
    int rightqrg;       // qrg of the right margin of the bitmap
    int resolution;     // resolution of the fft data in Hz per value
    int toprow;         // start drawing here, keep lines above toprow clear for the title
    int tunedQRG;    // base QRG for titles
} WF_DATA;

WF_DATA wfvars[WFID_MAX];

void init_wf_univ()
{
    for(int i=0; i<WFID_MAX; i++)
        wfvars[i].dg_first = 1;
}

void drawWF(int id, double *fdata, int cnt, int wpix, int hpix, unsigned int _realqrg, int _rightqrg, int res, int _tunedQRG, char *_fn)
{
    wfvars[id].fftdata = fdata;
    wfvars[id].fftcnt = cnt;
    wfvars[id].pic_width = wpix;
    wfvars[id].pic_height = hpix;
    strcpy(wfvars[id].filename,_fn);
    wfvars[id].realqrg = _realqrg;
    wfvars[id].rightqrg = _rightqrg;
    wfvars[id].resolution = res;
    wfvars[id].toprow = 20;    // lines above toprow are used for the titles, below toprow for the moving waterfall
    wfvars[id].tunedQRG = _tunedQRG;
    
    // scale the fft data to match antenna dBm
    scaleSamples(fdata, cnt); 
    
    if(hpix > 1)
    {
        // if the requested height of the bitmap is > 1, draw a bitmap
        
        // filename of a temporary bitmap
        // make sure the /tmp directory is a RAM disk if you run the system on an SD card !
        char fn[512];
        sprintf(fn,"/tmp/w%d.bmp",id);
        
        // read the bitmap (if exists)
        gdImagePtr im = gdImageCreateFromFile(fn);
        if(im && wfvars[id].dg_first == 0)
        {
            drawWFimage(id, im, fn);
            // free ressources
            gdImageDestroy(im);
        }
        else
        {
            wfvars[id].dg_first = 0;
            // image file does not exist, create an empty image
            printf("Create new WF image: %s\n",fn);
            
            // create Image
            gdImagePtr im = gdImageCreate(wpix,hpix);      
            // black background
            gdImageColorAllocate(im, 0, 0, 0);  
            // draw frequency titles
            drawQrgTitles(id,im);
            // write to file
            gdImageFile(im, fn);
            // free ressources
            gdImageDestroy(im);
        }
           
        // the temporary bmp is needed because we need every pixel for copying
        // now the bmp image is ready convert it to jpg
        gdImagePtr convim = gdImageCreateFromFile(fn);
        if(convim)
        {
            gdImageFile(convim, wfvars[id].filename);
            gdImageDestroy(convim);
        }
    }
    else
    {
        // special handling for height=1px, one WF line
        // this is used if the waterfall is generated somewhere else
        // in this case generate a dataset with some header information
        // followed by the pixels and their colors (the pixel color is an index
        // into a palette, see color.c)

        // save the data in /tmp/wfline.pix in this format:
        // 1 byte: ID counter
        // 1 byte: waterfall ID
        // 4 byte: eal qrg of the SDR's hardware tuner in Hz
        // 4 byte: right margin frequency in Hz
        // 4 byte: tuner frequency in Hz
        // 4 byte: resolution Hz per pixel
        // 4 byte: offset to tuner frequency in Hz
        // followed by the dBm data, 1 byte per pixel holding the dBm value
        int right = _rightqrg / res;
        unsigned char wfdata[right+22];
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
        wfdata[idx++] = _rightqrg >> 24;
        wfdata[idx++] = _rightqrg >> 16;
        wfdata[idx++] = _rightqrg >> 8;
        wfdata[idx++] = _rightqrg;
        
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
        // which is 10489.525 MHz (DISPLAYED_FREQUENCY_KHZ)
        // if a freq was recved via CIV, enter it here
        if(civ_active != 0 && civ_freq != 0)
        {
            foffset = civ_freq - TUNED_FREQUENCY;
        }
        
        wfdata[idx++] = foffset >> 24;
        wfdata[idx++] = foffset >> 16;
        wfdata[idx++] = foffset >> 8;
        wfdata[idx++] = foffset;

        // draw pixel per pixel
        for(int i=0; i<right; i++)
        {
            // fdata is the dbm value of the received signal, a negative number
            // put it into the data string as positive numer
            wfdata[idx] = (unsigned char)(-fdata[i]);
            idx++;
            if(idx >= sizeof(wfdata))
                break;
        }
        
        // wfdata with length cnt is now filled with data
        // give it to the WebSocket Server
        ws_send(wfdata,idx,id);
    }
}

/*
 * draw the waterfall image is done in these steps:
 * - create a new empty destination image
 * - copy the title from the existing image to this new image (copy unmodified, as it is)
 * - copy the existing waterfall picture, one line to the bottom, which creates the moving picture
 * - add the new data in the top line
 */
void drawWFimage(int id, gdImagePtr im, char *fn)
{
    // create destination image
    gdImagePtr dst = gdImageCreate(wfvars[id].pic_width, wfvars[id].pic_height);
    // allocate the colors
    allocatePalette(dst);
    
    // draw the existing image into the new destination image
    // Title: copy it as it is
    gdImageCopy(dst,im,0,0,0,0,wfvars[id].pic_width,wfvars[id].toprow); 
    // WF: shifted by 1 line (keep the top line empty)
    gdImageCopy(dst,im,0,wfvars[id].toprow+1,0,wfvars[id].toprow,wfvars[id].pic_width,(wfvars[id].pic_height-wfvars[id].toprow)-1); 
    // and draw the top line with the new fft data
    drawFFTline(id, dst);
    // write to file
    gdImageFile(dst, fn);
    // free ressources
    gdImageDestroy(dst);
}

/*
 * using the new FFT data, create a new waterfall line
 */
void drawFFTline(int id, gdImagePtr dst)
{
    int left = 0;
    int right = wfvars[id].rightqrg / wfvars[id].resolution;
    int width = right-left;
    
    // calculate pixel colors
    calcColorParms(id,left,right,wfvars[id].fftdata); 
    
    // calculate position of vertical lines
    int fullwidth = wfvars[id].rightqrg;
    char snum[50];
    sprintf(snum,"%d",fullwidth);
    for(int i=1; i<strlen(snum); i++) 
        snum[i] = '0';
    
    int numOfMarkers = 4;
    int markersteps = atoi(snum) / numOfMarkers;
    
    // draw pixel per pixel
    //printf("%d %d %d %d\n",leftqrg,rightqrg,pic_width,width);
    for(int i=left; i<right; i++)
    {
        // scale the frequency to pixel position
        int xs = ((i-left)*wfvars[id].pic_width)/width;
        int xe = (((i+1)-left)*wfvars[id].pic_width)/width;
        
        // at a 100kHz boundary make a thick white line
        if(!((i*wfvars[id].resolution) % markersteps) && (i != left) && (i != right))
        {
            gdImageLine(dst, xs, wfvars[id].toprow, xe, wfvars[id].toprow, 253);
        }
        else
        {
            // and draw the pixel in the color corresponding to the level
            gdImageLine(dst, xs, wfvars[id].toprow, xe, wfvars[id].toprow, getPixelColor(id,wfvars[id].fftdata[i]));
        }
    }
}

/*
 * draw the title
 */
void drawQrgTitles(int id, gdImagePtr dst)
{
char s[50];
    
    allocatePalette(dst);
    
    int left = 0;
    int right = wfvars[id].rightqrg / wfvars[id].resolution;
    int width = right-left;
    
    // calculate position of vertical lines
    int fullwidth = wfvars[id].rightqrg;
    char snum[50];
    sprintf(snum,"%d",fullwidth);
    for(int i=1; i<strlen(snum); i++) 
        snum[i] = '0';
    
    int numOfMarkers = 4;
    int markersteps = atoi(snum) / numOfMarkers;
    
    for(int i=left; i<right; i++)
    {
        // insert title text
        if(!((i*wfvars[id].resolution) % markersteps) && (i != left) && (i != right))
        {
            // scale the frequency to pixel position
            int xs = ((i-left)*wfvars[id].pic_width)/width;
            
            sprintf(s,"%.6f",(double)((double)wfvars[id].tunedQRG + i*(double)wfvars[id].resolution)/1e3);
            gdImageString (dst,gdFontGetSmall(),xs-25,2,(unsigned char *)s,253);
        }
    }
}

/*
 * scale FTT values
 * 
 * a change of 6 dBm at the antenna input must result in *2 or /2 of
 * the spectrum needle.
 * To match the FFT output with this requirement we must
 * use a log(base 1,122). I don't know why this works, but I have
 * carefully tested and simulated this formula.
 * 
 * Overwrites "samples" !
 * */

// calibration value to match the spectrum display to the antenna input
double refminus80dBm = 0;

void scaleSamples(double *samples, int numSamples)
{
    double log1122 = log(1.122);
    double maxval = (double)(log(32768.0*(double)numSamples) / log1122);
    
    for(int i=0; i<numSamples; i++)
    {
        double dval = samples[i];
        
      //  printf("%.0f ",dval);            

        
        // a log of 0 is not possible, so make it to 1
        if (dval < 1) dval = 1;    
        
        // this formula is a log to the base of 1.122
        dval = (double)(log(dval) / log1122);
        
        /*
         * lowest value:
         *  the lowest FFT output value may be 0 (corrected to 1 above), 
         *  the result of the log is 1
         * highers value:
         *  the max. value of the FFT output can be 32768*(uFFT_rate/2)
         * 
         * Example: uFFT_rate = 48k, maxVal=786.432.000
         * the above log results in: 178
         * therefore: the maximum dynamic range in this case is 178 dBm
         * */
        
        // turn into a negative dBm value
        dval = dval - maxval;
        
        // at this point "dval" matches the dBm value at the antenna input
        // but we need to add a correction value which has to be calibrated
        // at -80dBm. 
        // !!! all this works ONLY if the receiver AGC is switched OFF !!!
        dval += refminus80dBm;
        
        samples[i] = dval;
    }
}
