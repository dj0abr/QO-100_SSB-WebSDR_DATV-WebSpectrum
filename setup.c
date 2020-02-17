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
* setup.c ... configuration parameters and functions
* 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include "qo100websdr.h"
#include "beaconlock.h"
#include "civ.h"
#include "websocket/websocketserver.h"

int read_config();

char callsign[20] = "CALLSIGN";
int64_t lnb_lo = 0;
uint32_t lnb_crystal = DEFAULT_LNB_CRYSTAL;
int32_t tuned_frequency = 0;
int32_t lnb_multiplier = DEFAULT_LNB_MULTIPLIER;
int32_t downmixer_outqrg = DEFAULT_DOWNMIXER_OUTQRG;
int32_t minitiouner_offset = 0;
char minitiouner_ip[20] = {"192.168.0.25"};
int minitiouner_port = 6789;
int minitiouner_local = 1;
int websock_port = DEFAULT_WEBSOCK_PORT;
int allowRemoteAccess = 1;
int configrequest = 0;
int retune_setup = 0;
int tx_correction = 0;
int icom_satmode = 0;

void calc_setup()
{
int ret = 0;

    ret = read_config();
    if(ret == 0)
    {
        // keep default values from qo100websdr.h as assigned above
        printf("using default configuration\n");
    }
    
    if(ret == 1)
    {
        // new frequency settings
        // re-tune required
   
        if(downmixer_outqrg == 0)
        {
            // LO of the LNB's internal mixer in Hz
            lnb_lo = (((int64_t)lnb_crystal * (int64_t)lnb_multiplier)/(int64_t)1000);
        }
        else
        {
            // sum-LO of the complete chain: LNB and Downmixer in Hz
            lnb_lo = (((int64_t)10489 - (int64_t)downmixer_outqrg)*(int64_t)1000000);
        }
        
        #ifdef WIDEBAND
            // default SDR tuner frequency in Hz, which must be the middle of the spectrum
            tuned_frequency = (int32_t)((((int64_t)LEFT_MARGIN_QRG_KHZ + (((int64_t)WF_RANGE_HZ/(int64_t)1000) / (int64_t)2)) * (int64_t)1000) - lnb_lo);
            minitiouner_offset = ((lnb_crystal/1000000)*lnb_multiplier);
        #else
            // default SDR tuner frequency in Hz, which must be the left margin of the spectrum
            tuned_frequency = (((int64_t)LEFT_MARGIN_QRG_KHZ * (int64_t)1000) - lnb_lo);
        #endif
    }
    
    if(ret == 2)
    {
        // no re-tuning required
    }
    
    configrequest = 1;  // send config to browser
}

// we need different config files for root and user
// otherwise a user would not be able to write a cfg file created by root
#define CFG_FILENAME        "wb.cfg"
#define CFG_FILENAME_ROOT   "wb_admin.cfg"

// save config values in a readable format in file: wb.cfg
void save_config()
{
FILE *fw;
char cfgfilename[256];
int i;

    if((i=getuid()))
        // we are a normal user
        strcpy(cfgfilename,CFG_FILENAME);
    else
        // we are root
        strcpy(cfgfilename,CFG_FILENAME_ROOT);

    printf("save configuration\n");
    if((fw = fopen(cfgfilename,"wb")))
    {
        fprintf(fw,"callsign:%s\n",callsign);
        fprintf(fw,"lnb_crystal:%lld\n",(long long int)lnb_crystal);
        fprintf(fw,"lnb_multiplier:%lld\n",(long long int)lnb_multiplier);
        fprintf(fw,"downmixer_outqrg:%lld\n",(long long int)downmixer_outqrg);
        fprintf(fw,"minitiouner_ip:%s\n",minitiouner_ip);
        fprintf(fw,"minitiouner_port:%lld\n",(long long int)minitiouner_port);
        fprintf(fw,"minitiouner_local:%lld\n",(long long int)minitiouner_local);
        fprintf(fw,"websock_port:%lld\n",(long long int)websock_port);
        fprintf(fw,"allowRemoteAccess:%lld\n",(long long int)allowRemoteAccess);
        fprintf(fw,"CIV_address:%lld\n",(long long int)civ_adr);
        fprintf(fw,"tx_correction:%lld\n",(long long int)tx_correction);
        fprintf(fw,"icom_satmode:%lld\n",(long long int)icom_satmode);
        

        fclose(fw);
    }
    else
        printf("ERROR: cannot write config file: %s\n",CFG_FILENAME);
}

char *getCfgFilename()
{
static char cfgfilename[256];
int i;

    if((i=getuid()))
    {
        // we are a normal user
        strcpy(cfgfilename,CFG_FILENAME);
        // if no cfg file is available, use a cfg file previously created by root
        FILE *fp = fopen(CFG_FILENAME,"rb");
        if(fp)
            fclose(fp);
        else
        {
            printf("using config created by root: ");
            strcpy(cfgfilename,CFG_FILENAME_ROOT);
        }
    }
    else
    {
        // we are root
        strcpy(cfgfilename,CFG_FILENAME_ROOT);
    }
    
    return cfgfilename;
}

int readnum(uint64_t *pnum, char *elem)
{
FILE *fr;
char s[100];
char selem[100];
char snum[100];

    if((fr = fopen(getCfgFilename(),"rb")))
    {
        while(fgets(s,100,fr) != NULL)
        {
            // search delimiter ':'
            char *p = strchr(s,':');
            if(p == NULL)
            {
                // not found, format error in config file
                printf("config file format error\n");
                fclose(fr);
                return 0;
            }
            // split line
            *p = 0;
            strcpy(selem,s);
            strcpy(snum,p+1);
            
            if(!strncmp(selem,elem,strlen(elem)))
            {
                *pnum = atoll(snum);
                printf("cfg file read: %s = %lld\n",selem,(long long)(*pnum));
                fclose(fr);
                return 1;
            }
        }

        fclose(fr);
    }
    else
    {
        printf("no config file, use defaults\n");
    }
    
    printf("config file: element %s not found, using default config\n",elem);
    return 0;
}

char *readstring(char *elem)
{
FILE *fr;
char s[100];
char selem[100];
static char sstr[100];

    if((fr = fopen(getCfgFilename(),"rb")))
    {
        while(fgets(s,100,fr) != NULL)
        {
            // search delimiter ':'
            char *p = strchr(s,':');
            if(p == NULL)
            {
                // not found, format error in config file
                printf("config file format error\n");
                fclose(fr);
                return NULL;
            }
            // split line
            *p = 0;
            strcpy(selem,s);
            strcpy(sstr,p+1);
            
            if(!strncmp(selem,elem,strlen(elem)))
            {
                // strip CRLF
                char *p = strchr(sstr,'\n');
                if(p) *p=0;
                p = strchr(sstr,'\r');
                if(p) *p=0;
                printf("cfg file read: %s = %s\n",selem,sstr);
                fclose(fr);
                return sstr;
            }
        }

        fclose(fr);
    }
    else
    {
        printf("no config file, use defaults\n");
        return NULL;
    }
    
    printf("config file: element %s not found, using default config\n",elem);
    return NULL;
}

// 0=no config found use default, 1=new config read re-tuning required, 2=as 1 but no need to re-tune
int read_config()
{
uint64_t num = 0;
static uint32_t old_lnb_crystal = 0;
static int32_t old_lnb_multiplier = 0;
static int32_t old_downmixer_outqrg = 0;

    char *p = readstring("callsign");
    if(p != NULL) strcpy(callsign,p); else return 0;
    if(readnum(&num,"lnb_crystal")) lnb_crystal = num; else return 0;
    if(readnum(&num,"lnb_multiplier")) lnb_multiplier = num; else return 0;
    if(readnum(&num,"downmixer_outqrg")) downmixer_outqrg = num; else return 0;
    p = readstring("minitiouner_ip");
    if(p != NULL) strcpy(minitiouner_ip,p); else return 0;
    if(readnum(&num,"minitiouner_port")) minitiouner_port = num; else return 0;
    if(readnum(&num,"minitiouner_local")) minitiouner_local = num; else return 0;
    if(readnum(&num,"websock_port")) websock_port = num; else return 0;
    if(readnum(&num,"allowRemoteAccess")) allowRemoteAccess = num; else return 0;
    if(readnum(&num,"CIV_address")) civ_adr = num; else return 0;
    if(readnum(&num,"tx_correction")) tx_correction = num; else return 0;
    if(readnum(&num,"icom_satmode")) icom_satmode = num; else return 0;
    
    if( old_lnb_crystal != lnb_crystal ||
        old_lnb_multiplier != lnb_multiplier ||
        old_downmixer_outqrg != downmixer_outqrg)
    {
        printf("new QRG settings, re-tune SDR\n");
        old_lnb_crystal = lnb_crystal;
        old_lnb_multiplier = lnb_multiplier;
        old_downmixer_outqrg = downmixer_outqrg;
        return 1;
    }
        
    return 2;
}

unsigned char cfgdata[256];
int idx = 0;

void insertCfg4(uint32_t v)
{
    cfgdata[idx++] = v >> 24;
    cfgdata[idx++] = v >> 16;
    cfgdata[idx++] = v >> 8;
    cfgdata[idx++] = v & 0xff;
}

// strings have a fixed len of CFGSTRLEN bytes, independent of the real length of s
#define CFGSTRLEN 20
void insertCfgString(char *s)
{
    memset(cfgdata+idx,0,CFGSTRLEN);
    memcpy(cfgdata+idx,s,strlen(s));
    idx += CFGSTRLEN;
}

void sendConfigToBrowser()
{
    idx = 2;
    
    configrequest = 0;
    printf("Build config string\n");
    
    insertCfgString(callsign);
    insertCfg4(websock_port);
    insertCfg4(allowRemoteAccess);
    if(lnb_multiplier == 390000)
        insertCfg4(25);
    else
        insertCfg4(27);
    insertCfg4(lnb_crystal);
    insertCfg4(downmixer_outqrg);
    insertCfgString(minitiouner_ip);
    insertCfg4(minitiouner_port);
    insertCfg4(minitiouner_local);
    insertCfg4(civ_adr);
    insertCfg4(tx_correction);
    insertCfg4(icom_satmode);
    
    ws_send_config(cfgdata, idx);
}


char *hp_elem;
char *he_elem;

int getNextElement(char *s)
{
    he_elem = strchr(hp_elem,';');
    if(!he_elem) 
    {
        printf("getConfigfromBrowser: format error\n");
        return 0;
    }
    *he_elem = 0;
    if(strlen(hp_elem) > 20)
    {
        printf("getConfigfromBrowser: length error\n");
        return 0;
    }
    strcpy(s,hp_elem);
    hp_elem = he_elem+1;
    return 1;
}

// called if the user closes the setup window
// Format: "CALLSIGN;8091;1;25;24000000;0;192.168.0.25;6789;1"
void getConfigfromBrowser(char *scfg)
{
char s[20] = {0};
static uint32_t old_lnb_crystal = 0;
static int32_t old_lnb_multiplier = 0;
static int32_t old_downmixer_outqrg = 0;

    hp_elem = scfg;
    
    
    if(getNextElement(callsign) == 0) return;
    
    if(getNextElement(s) == 0) return;
    websock_port = atoi(s);

    if(getNextElement(s) == 0) return;
    allowRemoteAccess = atoi(s);

    if(getNextElement(s) == 0) return;
    int i = atoi(s);
    if(i==25) lnb_multiplier=390000; else lnb_multiplier=361111;

    if(getNextElement(s) == 0) return;
    lnb_crystal = atoi(s);

    if(getNextElement(s) == 0) return;
    downmixer_outqrg = atoi(s);
    
    if(getNextElement(minitiouner_ip) == 0) return;

    if(getNextElement(s) == 0) return;
    minitiouner_port = atoi(s);

    if(getNextElement(s) == 0) return;
    minitiouner_local = atoi(s);
    
    if(getNextElement(s) == 0) return;
    civ_adr = atoi(s);
    
    if(getNextElement(s) == 0) return;
    tx_correction = atoi(s);
    
    if(getNextElement(s) == 0) return;
    icom_satmode = atoi(s);
    
    if( old_lnb_crystal != lnb_crystal ||
        old_lnb_multiplier != lnb_multiplier ||
        old_downmixer_outqrg != downmixer_outqrg)
    {
        old_lnb_crystal = lnb_crystal;
        old_lnb_multiplier = lnb_multiplier;
        old_downmixer_outqrg = downmixer_outqrg;
        
        retune_setup = 1;   // calc and install new tuner settings
    }
    
    
}
