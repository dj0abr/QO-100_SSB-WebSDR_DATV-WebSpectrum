#include <gd.h>

void allocatePalette(gdImagePtr img);
void calcColorParms(int id, int fstart, int fend, double *d);
int getPixelColor(int id, float val);
