int readCIVmessage(int reti);
void civ_ptt(int onoff, unsigned char civad);
void civ_setQRG(int freq);
void civ_queryQRG();
void civ_queryTXQRG();
void civ_selMainSub(int mainsub);

#define MAXCIVDATA 30

extern unsigned int civ_freq;
extern unsigned int civ_subfreq;
extern int civ_active;
extern int civ_confirm;
extern int civ_adr;
