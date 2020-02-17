#include "qo100websdr.h"

int init_SDRplay();
void remove_SDRplay();
void streamCallback(short *xi, short *xq, unsigned int firstSampleNum,
    int grChanged, int rfChanged, int fsChanged, unsigned int numSamples,
    unsigned int reset, unsigned int hwRemoved, void *cbContext);
void gainCallback(unsigned int gRdB, unsigned int lnaGRdB, void *cbContext);
void setTunedQrgOffset(int hz);
void reset_Qrg_SDRplay();


