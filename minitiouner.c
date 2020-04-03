/*
 * Remote Control for minitiouner
 *
 * send via UDP to the PC where minitiouner is running, port: 6789
 * 
 * using a bash command:
 * 
 * echo "[GlobalMsg],Freq=10492527,Offset=9360000,Doppler=0,Srate=2000,WideScan=0,LowSR=0,DVBmode=Auto,FPlug=A,Voltage=0,22kHz=Off" > /dev/udp/IPADR/PORT
 * 
 * minimal sentence accepted my minitiouner software:
 * 
 * "[GlobalMsg],Freq=10492527,Offset=9360000,Doppler=0,Srate=2000,DVBmode=Auto"
 * 
 * */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/file.h>
#include <pthread.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <errno.h>
#include "../qo100websdr.h"
#include "../setup.h"

int mt_port;
int mt_sock;

void init_udp_minitiouner()
{
	// öffne NUM_OF_PIPES UDP ports mit zufälliger Portnummer
	srand(time(NULL));
    mt_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (mt_sock == -1)
    {
        printf("Failed to create UDP socket,errno=%d\n", errno);
        return;
    }

    struct sockaddr_in mysockadr;
    memset(&mysockadr, 0, sizeof(struct sockaddr_in));
    mysockadr.sin_family = AF_INET;
    mt_port = (rand() % 1000) + 40000;
    mysockadr.sin_port = htons(mt_port);
    printf("open sock: %d\n", mt_port);
    mysockadr.sin_addr.s_addr = INADDR_ANY;

    if (bind(mt_sock, (struct sockaddr *)&mysockadr, sizeof(struct sockaddr_in)) != 0)
    {
        printf("Failed to bind PIPE socket, errno=%d\n", errno);
        close(mt_sock);
        mt_sock = -1;
        return;
    }
}

void remove_udp_minitiouner()
{
    if (mt_sock != -1)
        close(mt_sock);
}

char write_udppipe(unsigned char *data, int len)
{
	if (mt_sock != -1)
	{
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(minitiouner_port);
		sin.sin_addr.s_addr = inet_addr(minitiouner_ip);
		sendto(mt_sock, (char *)data, len, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	}
	return 1;
}

void setMinitiouner(char *msg)
{
char s[1000];
char qrg[20];
char bw[20];

    memcpy(qrg,msg,8);
    qrg[8] = 0;
    char *hp = strchr(msg,',');
    if(hp)
    {
        hp++;
        if(strlen(hp) > 1 && strlen(hp) < 6)
        {
            strcpy(bw,hp);
            
            sprintf(s,"echo \"[GlobalMsg],Freq=%8.8s,Offset=%d,Doppler=0,Srate=%s,DVBmode=Auto\n\" > /dev/udp/%s/%d",qrg,minitiouner_offset,bw,minitiouner_ip,minitiouner_port);
            //printf("send to Minitiouner: <%s>\n\n",s);
            
            write_udppipe((unsigned char *)s, strlen(s)+1); // +1 because we need to send including the null terminator
        }
    }
}
