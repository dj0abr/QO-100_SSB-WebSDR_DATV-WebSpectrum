int readCIVmessage(int reti);
void civ_ptt(int onoff, unsigned char civad);
void civ_setQRG(int freq);
void civ_queryQRG();

#define MAXCIVDATA 30

extern unsigned int civ_freq;
