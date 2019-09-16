/*
* Web based SDR Client for SDRplay and RTLsdr
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
* q100websdr.c ... file containing main() and calling all other functions 
* 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include <rtl-sdr.h>
#include "sdrplay.h"
#include "audio.h"
#include "wf_univ.h"
#include "setqrg.h"
#include "websocketserver.h"
#include "rtlsdr.h"
#include "fifo.h"
#include "ssbfft.h"
#include "wb_fft.h"
#include "downmixer.h"
#include "cat.h"

int hwtype = 0; // 1=playSDR, 2=rtlsdr
int samplesPerPacket;
char htmldir[256] = { "." };
int TUNED_FREQUENCY = _TUNED_FREQUENCY;

void sighandler(int signum)
{
	printf("signal %d, exit program\n",signum);
    #ifdef SDR_PLAY
    if(hwtype == 1) remove_SDRplay();
    #endif
    if(hwtype == 2) rtlsdr_close(0);
    exit(0);
}

void sighandler_mem(int signum)
{
	printf("memory error, signal %d, exit program\n",signum);
    #ifdef SDR_PLAY
    if(hwtype == 1) remove_SDRplay();
    #endif
    if(hwtype == 2) rtlsdr_close(0);
    exit(0);
}

// check if playSDReshail2 is already running
void isRunning()
{
    int num = 0;
    char s[256];
    sprintf(s,"ps -e | grep playSDReshail2");
    
    FILE *fp = popen(s,"r");
    if(fp)
    {
        // gets the output of the system command
        while (fgets(s, sizeof(s)-1, fp) != NULL) 
        {
            if(strstr(s,"playSDReshail2") && !strstr(s,"grep"))
            {
                if(++num == 2)
                {
                    printf("playSDReshail2 is already running, do not start twice !");
                    pclose(fp);
                    exit(0);
                }
            }
        }
        pclose(fp);
    }
}

// cleans white spaces from beginning, middle and end of a string
char *cleanString(char *str, int cleanspace)
{
static char hs[256];
char *hp = str;
int idx = 0;

    // putze den Anfang
    while(*hp == ' ' || *hp == ',' || *hp == '\n' || *hp == '\r' || *hp == '\'' || *hp == '\"')
    {
 hp++;
    }
    
    // putze die Mitte
    if(cleanspace)
    {
 while(*hp)
 {
     if(*hp != ' ' && *hp != ',' && *hp != '\n' && *hp != '\r' && *hp != '\'' && *hp != '\"')
  hs[idx++] = *hp;
     hp++;
 }
    }
    else
    {
 while(*hp)
     hs[idx++] = *hp++;
    }
    
    // putze das Ende
    hp = hs+idx-1;

    while(*hp == ' ' || *hp == ',' || *hp == '\n' || *hp == '\r' || *hp == '\'' || *hp == '\"')
    {
 *hp = 0;
 hp--;
    }
    

    hs[idx] = 0;

    return hs;
}

int sret;
void *pret;

void searchHTMLpath()
{
    if(htmldir[0] == '.')
    {
        // search for the apache woking directory
        sret = system("find  /srv -xdev -name htdocs  -print > pfad 2>/dev/null");
        sret = system("find  /var -xdev -name htdocs  -print >> pfad 2>/dev/null");
        sret = system("find  /var -xdev -name html  -print >> pfad 2>/dev/null");
        sret = system("find  /usr -xdev -name htdocs  -print >> pfad 2>/dev/null");
        // if the directory was found its name is in file: "pfad"
        FILE *fp=fopen("./pfad","r");
        if(fp)
        {
            char p[256];
            pret = fgets(p,255,fp);
            char *cp= cleanString(p,0);
            if(strlen(cp)>3)
            {
                strcpy(htmldir,cp);
                printf("Webserver Path: %s",htmldir);
            }
            else
            {
                printf("Path to apache webserver files not found");
                exit(0);
            }
            
            fclose(fp);
        }
        else
        {
            printf("Path to apache webserver files not found");
            exit(0);
        }
    }
}

void installHTMLfiles()
{
    // the HTML files are located in the html folder below the es folder
    // copy these files into the Apache HTML folder
    int i;
    if ((i=getuid()))
    {
        printf("\nYou are not root, HTML files are not automatically copied into the Apache folder!\n");
        return;
    }
    
    char fn[512];
    snprintf(fn,sizeof(fn),"cp ./html/* %s",htmldir);
    printf("copy Web Site files to: %s",htmldir);
    FILE *fp = popen(fn,"r");
    if(fp)
    {
        // Liest die Ausgabe von cp
        while (fgets(fn, sizeof(fn)-1, fp) != NULL) 
        {
            //printf("Output: %s\n",fn);
        }
        pclose(fp);
    }
}

void usage()
{
    printf("\nusage:\n");
    printf("./playSDReshail2  -f  frequency\n");
    printf("frequency ... frequency of the CW beacon minus 25 kHz\n");
    printf("using a normal LNB the frequency is 739525000\n");
    printf("using a down mixer: enter the output frequency of the down mixer, i.e.: 144525000\n");
}

int main(int argc, char *argv[])
{
int c;

    while((c = getopt(argc, argv, "f:")) !=-1) 
    {
        switch (c) {
            case 'f':
                TUNED_FREQUENCY = strtod(optarg,NULL); // in Hz
                if(TUNED_FREQUENCY < 100000 || TUNED_FREQUENCY > 1500000000)
                {
                    printf("\nenter QRG in Hz. i.e.: 144525000 or 739525000 or similar\n\n");
                    return 1;
                }
                break;
            case '?':
                usage();
                return 1;
        }
    }
    
    if(argc == 1)
    {
        usage();
        return 1;
    }
    
    // check if it is already running, if yes then exit
    isRunning();
    
    // look for the apache HTML path
    searchHTMLpath();
    
    // Install or Update the html files
    installHTMLfiles();
    
    // make signal handler, mainly use if the user presses Ctrl-C
    struct sigaction sigact;
    sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	//sigaction(SIGPIPE, &sigact, NULL); // signal 13
    
    // switch off signal 13 (broken pipe)
    // instead handle the return value of the write or send function
    signal(SIGPIPE, SIG_IGN);
    
    /*struct sigaction sigact_mem;
    sigact_mem.sa_handler = sighandler_mem;
	sigemptyset(&sigact_mem.sa_mask);
	sigact_mem.sa_flags = 0;
    sigaction(SIGSEGV, &sigact_mem, NULL);*/
    
    printf("\nplaySDRweb parameters:\n\n");
    printf("SDR base QRG:    %d Hz\n",TUNED_FREQUENCY);
    printf("SDR sample rate: %d S/s\n",SDR_SAMPLE_RATE);
    printf("WF width:        %d Hz\n",WF_RANGE_HZ);
    printf("WF width:        %d pixel\n",WF_WIDTH);
    printf("1st downsampling:%d S/s\n",SAMPLERATE_FIRST);
    printf("1st decim. rate: %d\n",DECIMATERATE);
    printf("1st FFT resol.:  %d Hz\n",FFT_RESOLUTION);
    printf("1st FTT smp/pass:%d\n",SAMPLES_FOR_FFT);
    printf("SSB audio rate  :%d\n",SSB_RATE);
    printf("SSB audio decim :%d\n",SSB_DECIMATE);
    printf("2nd FFT resol.  :%d\n",FFT_RESOLUTION_SMALL);
    printf("2nd FTT smp/pass:%d\n",SAMPLES_FOR_FFT_SMALL);
    
    
    #ifdef WIDEBAND
        init_fwb();
    #else
        init_fssb();
    #endif // WIDEBAND

    
    downmixer_init();
    
    // init the FIFO
    initpipe();
    
    // init waterfall drawing
    init_wf_univ();
    
    // init the Websocket Server
    ws_init();
    
    // init audio output
    #ifdef SOUNDLOCAL
        init_soundprocessing();
    #endif
        
    // init CAT interface
    cat_init();
    
    printf("\nInitialisation complete, system running ... stop with Ctrl-C\n");
    
    // init SDRplay hardware
    // this MUST be the LAST initialisation because
    // the callback starts immediately after init_SDRplay
	#ifdef WIDEBAND
		// only SDRplay can be used in wideband mode
		init_SDRplay();
		hwtype = 1;
	#else
		// for NB transponder try RTLsdr first
		// and if not exists, try SDRplay
		if(init_rtlsdr())
		{
		    hwtype = 2;
		    samplesPerPacket = SAMPLES_PER_PASS;
		}

		#ifdef SDR_PLAY
		    if(hwtype == 0)
		    {
		        init_SDRplay();
		        hwtype = 1;
		    }
    	#endif
	#endif // WIDEBAND
       

    if(hwtype == 0)
    {
        printf("no SDR hardware found.\n");
        exit(0);
    }

    // infinite loop, 
    // stop program with Ctrl-C
    while(1)
    {
        set_frequency();
        
        usleep(1000);
    }
    
    removepipe();
    
    return 0;
}
