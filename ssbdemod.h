void ssbdemod(fftw_complex *cpout, int offset);
void *ssbdemod_thread(void *param);
void init_ssbdemod();

extern int audioon[MAX_CLIENTS];
