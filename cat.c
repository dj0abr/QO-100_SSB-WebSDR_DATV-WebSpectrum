/*
 * es'hail WebSDR
 * ======================
 * by DJ0ABR 
 * 
 * cat.c ... Transceiver CAT interface
 * 
 * mainly handles the serial interfaceand routes commends to the CAT modules
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

void closeSerial();
int activate_serial(char *serdevice, int baudrate);
void openSerialInterface();
int read_port();
void ser_loop();

void *catproc(void *pdata);
pthread_t cat_tid; 
int trx_frequency = 0;

#define SERSPEED_CIV B4800
#define SERSPEED_DDS B115200

int fd_ser = -1; // handle of the ser interface
char serdevice[20] = {"/dev/ttyUSB0"};
int ser_command = 0;    // 0=no action 1=queryQRG 2=setPTT 3=releasePTT 4=setQRG
int ser_status = 0;
int setIcomFrequency = 0;   // 0=do nothing, 1= a click in the waterfall sets the icom transceiver frequency
int useCAT = 0;         // 0= don't use the serial port, 1= use the serial port for Icom CIV

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

void *catproc(void *pdata)
{
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
            // open the serial interface and handle all messages through the CIV module
            if(fd_ser == -1)
            {
                // serial IF is closed, try to open it
                sleep(1);
                openSerialInterface(SERSPEED_CIV);
            }
            else
            {
                // serial IF is open, read one byte and pass it to the CIV message receiver routine
                int reti = read_port();
                if(reti == -1) 
                    usleep(10000);  // no data received
                else
                    readCIVmessage(reti);
            }
        }
 
        ser_loop();
        usleep(100);
    }
    
    printf("exit serial thread\n");
    closeSerial();
    pthread_exit(NULL);
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
int activate_serial(char *serdevice,int baudrate)
{
	struct termios tty;
    
	closeSerial();
	fd_ser = open(serdevice, O_RDWR | O_NDELAY);
	if (fd_ser < 0) {
		//printf("error when opening %s, errno=%d\n", serdevice,errno);
		return -1;
	}

	if (tcgetattr(fd_ser, &tty) != 0) {
		printf("error %d from tcgetattr %s\n", errno,serdevice);
		return -1;
	}

	cfsetospeed(&tty, baudrate);
	cfsetispeed(&tty, baudrate);

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

int ttynum = 0;

void openSerialInterface(int baudrate)
{
int ret = -1;
    
    while(ret == -1)
    {
		printf("try to open %s\n",serdevice);
        ret = activate_serial(serdevice,baudrate);
        if(ret == 0) 
        {
            printf("%s is now OPEN\n",serdevice);
            sleep(1);
			ser_status = 0;
            break;
        }
        fd_ser = -1;
        sleep(1);
        if(++ttynum >= 4) ttynum = 0;
        sprintf(serdevice,"/dev/ttyUSB%d",ttynum);
    }
}

// read one byte non blocking
int read_port()
{
static unsigned char c;
static int rxfail = 0;

	//printf("ICOM -> PC: ");

    int rxlen = read(fd_ser, &c, 1);
    
    if(rxlen == 0)
    {
		//printf("no data, rxfail: %d\n",rxfail);
		rxfail++;
		if(rxfail >= 100)
		{
			printf("no data after 100 tries, close interface\n");
			closeSerial();
			ser_status = 0;
			rxfail = 0;
			if(++ttynum >= 4) ttynum = 0;
				sprintf(serdevice,"/dev/ttyUSB%d",ttynum);
		}
		if(rxfail == 40 || rxfail == 60 || rxfail == 80)
		{
			// new qrg request
			ser_status = 0;
			ser_command = 0;
		}
        return -1;
    }
	else
	{
	    //printf("%d, %02X\n",rxlen,c);
	}

	rxfail = 0;
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

void ser_loop()
{
    // if(ser_command == 1) printf("civ_freq:%d %d %d\n",civ_freq,ser_status,ser_command);
    
    switch (ser_status)
    {
        case 0: 
                if(ser_command == 0)
                {
                    // read the MAIN VFO frequency
                    // this must be the selected default VFO
                    // because frequent VFO change disturbs the Icom user interface
                    civ_confirm = 0;
                    civ_queryQRG();
                    ser_status = 1;
                }
                
                if(ser_command == 2)
                {
                    if(setIcomFrequency && civ_freq != 0)
                    {
                        // GUI sends frequency
			printf("civ_freq:%d\n",civ_freq);
                        ser_command = 0;
                        civ_confirm = 0;
                        civ_selMainSub(1);
                        ser_status = 11;
                    }
                    else
                    {
                        // if civ_freq unknown, do nothing
                        ser_command = 0;
                    }
                }
                break;
                
        case 1: // wait for frequency
                if(civ_confirm == 1)
                {
                    ser_status = 0;
                }
                break;

        case 11: // wait for confirmation of Sub-VFO selection
                if(civ_confirm == 1)
                {
                    // the new calculated frequency
                    int tx_qrg = trx_frequency - TUNED_FREQUENCY + TRANSMIT_FREQUENCY;
                    // add user offset
                    //tx_qrg += txoffset;
                    // set the QRG (the SUB VFO is currently selected)
                    civ_confirm = 0;
                    civ_setQRG(tx_qrg);
                    
                    ser_status = 14;
                }
                break;
                
        case 14:// wait for confirmation for setting TX frequency
                // then select MAIN VFO
                if(civ_confirm == 1)
                {
                    civ_confirm = 0;
                    civ_selMainSub(0);
                    ser_status = 15;
                }
                break;
                
        case 15:// wait for confirmation for selecting MAIN VFO
                // then set RX frequency
                if(civ_confirm == 1)
                {
                    civ_confirm = 0;
                    civ_setQRG(trx_frequency);
                    ser_status = 16;
                }
                break;
                
        case 16:// wait for confirmation for setting RX frequency
                if(civ_confirm == 1)
                {
                    ser_status = 0;
                    ser_command = 0;
                }
                break;
    }
}
