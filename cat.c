/*
 * es'hail WebSDR
 * ======================
 * by DJ0ABR 
 * 
 * cat.c ... Transceiver CAT interface
 * 
 * mainly handles the serial interface and routes commends to the CAT modules
 * 
 * */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/file.h>
#include <pthread.h>
#include "qo100websdr.h"
#include "cat.h"
#include "civ.h"
#include "setup.h"
#include "identifySerUSB.h"

void closeSerial();
int activate_serial();
void openSerialInterface();
int read_port();
void ser_loop();

void *catproc(void *pdata);
pthread_t cat_tid; 

#define SERSPEED_CIV B4800

int fd_ser = -1; // handle of the ser interface
char serdevice[20] = {"/dev/ttyUSB0"};
int ser_command = 0;    // 0=no action 1=queryQRG 2=setPTT 3=releasePTT 4=setQRG
int useCAT = 0;         // 0= don't use the serial port, 1= use the serial port for Icom CIV
int trx_frequency = 0;
int ttynum = 0;
int setIcomQRG = 0;

// creates a thread to run all serial specific jobs
// call this once after program start
// returns 0=failed, 1=ok
int cat_init()
{
    // automatically choose an USB port
    // start a new process
    int ret = pthread_create(&cat_tid,NULL,catproc, 0);
    if(ret)
    {
        printf("catproc NOT started\n");
        return 0;
    }
    
    printf("OK: catproc started\n");
    return 1;
}

int status = 0;

void *catproc(void *pdata)
{
int rxbyte = 0;
int respcnt = 0;
int tries = 0;
int cret = 0;

    pthread_detach(pthread_self());
    
    while(1)
    {
        if(useCAT == 0)
        {
            // TX is OFF, no action
            closeSerial();
            sleep(1);
            continue;
        }
        
        if(useCAT == 1)
        {
            // ICOM mode selected
            
            if(fd_ser == -1)
            {
                // somebody closed the serial IF, start from beginning
                status = 0;
            }
            
            //if(status >= 2)
            {
                // serial IF is open, receive one byte
                rxbyte = read_port();
                if(rxbyte != -1)
                {
                    //printf("rx: %d\n",rxbyte);
                }
            }
            
            switch (status)
            {
                case 0: // ser IF is closed, open it and look for an icom trx
                        printf("Open Serial Port\n");
                        sleep(1);
                        if(activate_serial() == 0)
                        {
                            status = 1;
                        }
                        else
                        {
                            // if open tty failed, then try the next ttyUSBx number
                            if(++ttynum >= 4) ttynum = 0;
                        }
                        break;
                        
                case 1: // serial IF is open, look for an icom TRX
                        printf("Query Icom\n");
                        // read VFO
                        civ_queryQRG();
                        civ_freq = 0;   // if icom answers, the civ_freq is set to the actual VFO frequency
                        respcnt = 0;
                        status = 2;
                        break;
                        
                case 2: // wait for a response from icom
                        cret = readCIVmessage(rxbyte);
                        
                        if(cret == 3 )
                        {
                            // got an answer from icom, so we found the transceiver
                            printf("Icom TRX found, goto normal processing\n");
                            status = 3; // normal processing
                            break;
                        }
                        
                        // this loop takes 100us, the icom may take up to 1000ms
                        // so we wait maximum 10000 loops
                        if(++respcnt > 10000)
                        {
                            // no answer from icom 
                            printf("no answer from Icom within 1000ms, try next serial port\n");
                            status = 0;
                            // try with the next serial port
                            closeSerial();
                            if(++ttynum >= 4) ttynum = 0;
                        }
                        
                        break;
                        
                case 3: // normal processing: query CIV and set frequency
                        if(setIcomQRG > 0)
                        {
                            // user requested to set the ICOM to a frequency
                            int civ_freq_MHz = civ_freq / 1000000;
                            int newQRG = civ_freq_MHz * 1000000 + setIcomQRG - 500000 - tx_correction;
                            civ_setQRG(newQRG);
                            setIcomQRG = 0;
                            break;
                        }
                        
                        usleep(100000); // query every 100ms
                        civ_queryQRG();
                        respcnt = 0;
                        tries = 0;
                        status = 4;
                        break;
                        
                case 4: // normal processing: wait for answer from icom
                        cret = readCIVmessage(rxbyte);
                        if(cret == 3)
                        {
                            // got an frequency from icom
                            // this QRG is read in wf_univ.c and sent to the browser
                            //printf("got QRG\n");
                            tries = 0;
                            respcnt = 0;
                            status = 3;
                            break;
                        }
                        
                        // no answer from icom 
                        if(++respcnt > 10000)
                        {
                            printf("no answer from Icom within 1s, try again\n");
                            if(++tries > 5)
                            {
                                printf("already 5 tries, Icom looks to be unavailable, search it again\n");
                                status = 0;
                                // try with the next serial port
                                closeSerial();
                                if(++ttynum >= 4) ttynum = 0;
                                break;
                            }
                            civ_queryQRG();
                            respcnt = 0;
                        }
                        
                        break;
            }
        }
 
        usleep(100);
    }
    
    printf("exit serial thread\n");
    closeSerial();
    pthread_exit(NULL);
}

int isTrxAvailable()
{
    if(status >= 3 && fd_ser != -1) return 1;
    return 0;
}

void closeSerial()
{
    if(fd_ser != -1) 
	{
        close(fd_ser);
	    fd_ser = -1;
		printf("serial interface closed\n");
	}
}

// Öffne die serielle Schnittstelle
int activate_serial()
{
	struct termios tty;
    char serdevice[30];
    
    char *sdev = get_serial_device_name();
    if(sdev == NULL)
    {
        // not automatically identified, use scanning
        snprintf(serdevice,19,"/dev/ttyUSB%d",ttynum);
        serdevice[19] = 0;
    }
    else
    {
        // found serial device
        memset(serdevice,0,20);
        strncpy(serdevice,sdev,19);
    }
    
	closeSerial();
    
    printf("Open: %s\n",serdevice);
    
	fd_ser = open(serdevice, O_RDWR | O_NDELAY);
	if (fd_ser < 0) {
		printf("error when opening %s, errno=%d\n", serdevice,errno);
		return -1;
	}

	if (tcgetattr(fd_ser, &tty) != 0) {
		printf("error %d from tcgetattr %s\n", errno,serdevice);
		return -1;
	}

	cfsetospeed(&tty, SERSPEED_CIV);
	cfsetispeed(&tty, SERSPEED_CIV);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
	tty.c_iflag &= ~ICRNL; // binary mode (no CRLF conversion)
	tty.c_lflag = 0;

	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 0; // 0=nonblocking read, 1=blocking read
	tty.c_cc[VTIME] = 0;

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);

	tty.c_cflag |= (CLOCAL | CREAD);

	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag |= CSTOPB;
	tty.c_cflag &= ~CRTSCTS;
    

	if (tcsetattr(fd_ser, TCSANOW, &tty) != 0) {
		printf("error %d from tcsetattr %s\n", errno, serdevice);
		return -1;
	}
	
	// set RTS/DTR
    int flags;
    ioctl(fd_ser, TIOCMGET, &flags);
    flags &= ~TIOCM_DTR;
    flags &= ~TIOCM_RTS;
    ioctl(fd_ser, TIOCMSET, &flags);
    
    printf("%s opened sucessfully\n",serdevice);
    
	return 0;
}

void direct_ptt(int onoff)
{
    // set/reset RTS/DTR
    int flags;
    ioctl(fd_ser, TIOCMGET, &flags);
    if(onoff)
    {
        flags |= TIOCM_DTR;
        flags |= TIOCM_RTS;
    }
    else
    {
        flags &= ~TIOCM_DTR;
        flags &= ~TIOCM_RTS;
    }
    ioctl(fd_ser, TIOCMSET, &flags);
}

// read one byte non blocking
int read_port()
{
static unsigned char c;

	//printf("ICOM -> PC: ");

    int rxlen = read(fd_ser, &c, 1);
    if(rxlen == 0)
    {
        return -1;
    }
	return (unsigned int)c;
}

// schreibe ein
void write_port(unsigned char *data, int len)
{
int debug = 0;

    if(debug)	
		printf("PC -> ICOM:\n");

    int ret = write(fd_ser, data, len);
    if(ret == -1)
    {
		closeSerial();
        fd_ser = -1;
    }

    if(debug)
	{
		for(int i=0; i<len; i++)
		{
		    printf("%02X ",data[i]);
		}
		printf(" ret: %d\n",ret);
	}
}

