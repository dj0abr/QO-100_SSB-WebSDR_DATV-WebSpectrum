void calc_setup();
void save_config();
void sendConfigToBrowser();
void getConfigfromBrowser(char *scfg);

extern int64_t lnb_lo;
extern int32_t tuned_frequency;
extern int32_t downmixer_outqrg;
extern int32_t minitiouner_offset;
extern char *minitiouner_ip;
extern int minitiouner_port;
extern int minitiouner_local;
extern int websock_port;
extern int allowRemoteAccess;
extern int configrequest;
extern int retune_setup;
extern int tx_correction;
extern int icom_satmode;
extern char mtip[];
