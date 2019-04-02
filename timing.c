
#include <stdio.h>
#include <time.h>
#include <sys/time.h>


void measure_samplerate(int id, int samp, int prt)
{
static unsigned long lastus[10];
static unsigned long samples[10];
static int f[10];
static int cnt[10];
static int first=1;

    if(first)
    {
        first = 0;
        for(int i=0; i<10; i++)
        {
            f[i] = 1;
            cnt[i] = 0;
        }
    }

    // measure sample rate
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    if(f[id])
    {
        f[id] = 0;
        lastus[id] = tv.tv_sec * 1000000 + tv.tv_usec;
        samples[id] = samp;
    }
    samples[id] += samp;
    unsigned long tnow = tv.tv_sec * 1000000 + tv.tv_usec;
    
    if(++(cnt[id]) >= prt)
    {
        cnt[id] = 0;
        printf("%d: %.3f\n",id,((samples[id]*1e6)/(tnow - lastus[id]))/1e6);
    }
}
