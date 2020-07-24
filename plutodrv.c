#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include "plutodrv.h"
#include "ssbfft.h"
#include "setup.h"
#include <iio.h>  // libiio-dev must be installed

int setup_pluto();
int pluto_create_rxthread();

/* common RX and TX streaming params */
struct stream_cfg {
	long long bw_hz; // Analog banwidth in Hz
	long long fs_hz; // Baseband sample rate in Hz
	long long lo_hz; // Local oscillator frequency in Hz
	const char* rfport; // Port name
};

enum iodev { RX, TX };

/* IIO structs required for streaming */
static struct iio_context *ctx   = NULL;
static struct iio_channel *rx0_i = NULL;
static struct iio_channel *rx0_q = NULL;
static struct iio_buffer  *rxbuf = NULL;

bool stop = false;
char tmpstr[64];
void *pluto_rxproc(void *pdata);
pthread_t pluto_tid; 

#define PLUTO_RXBUFSIZE 360000  // no of samples per buffer call

/*
 * we need 1.8MS sample rate
 * Pluto can only >2.5MS so we choose 3.8MS and decimate by 2
 */

char pluto_context_name[50];

int init_pluto()
{
    // scan for a pluto USB device
    char s[500];
    snprintf(s,499,"iio_info -s");
    s[499] = 0;
    FILE *fp = popen(s,"r");
    if(fp)
    {
        while (fgets(s, sizeof(s)-1, fp) != NULL) 
        {
            char *hp = strstr(s,"[usb:");
            if(hp)
            {
                hp += 1;
                char *he = strchr(hp,']');
                if(he)
                {
                    *he = 0;
                    strncpy(pluto_context_name,hp,49);
                    pluto_context_name[49] = 0;
                    printf("PLUTO found: <%s>\n",pluto_context_name);
                    if(setup_pluto() == 1)
                    {
                        if(pluto_create_rxthread() == 1)
                        {
                            printf("PLUTO initialized: OK\n");
                            return 1;
                        }
                    }
                    return 0;
                }
            }
        }
        pclose(fp);
    }
    else
        printf("ERROR: cannot execute ls command\n");
    
    return 0;
}

/* cleanup and exit */
void pluto_shutdown()
{
    stop = true;
    
	printf("* Destroying buffers\n");
	if (rxbuf) { iio_buffer_destroy(rxbuf); }

	printf("* Disabling streaming channels\n");
	if (rx0_i) { iio_channel_disable(rx0_i); }
	if (rx0_q) { iio_channel_disable(rx0_q); }

	printf("* Destroying context\n");
	if (ctx) { iio_context_destroy(ctx); }
	exit(0);
}

/* check return value of attr_write function */
static void errchk(int v, const char* what) {
	 if (v < 0) { fprintf(stderr, "Error %d writing to channel \"%s\"\nvalue may not be supported.\n", v, what); }
}

/* write attribute: long long int */
static void wr_ch_lli(struct iio_channel *chn, const char* what, long long val)
{
	errchk(iio_channel_attr_write_longlong(chn, what, val), what);
}

/* write attribute: string */
static void wr_ch_str(struct iio_channel *chn, const char* what, const char* str)
{
	errchk(iio_channel_attr_write(chn, what, str), what);
}

/* helper function generating channel names */
static char* get_ch_name(const char* type, int id)
{
	snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
	return tmpstr;
}

/* returns ad9361 phy device */
static struct iio_device* get_ad9361_phy(struct iio_context *ctx)
{
	struct iio_device *dev =  iio_context_find_device(ctx, "ad9361-phy");
	//ASSERT(dev && "No ad9361-phy found");
	return dev;
}

/* finds AD9361 streaming IIO devices */
static bool get_ad9361_stream_dev(struct iio_context *ctx, enum iodev d, struct iio_device **dev)
{
	switch (d) {
	case TX: *dev = iio_context_find_device(ctx, "cf-ad9361-dds-core-lpc"); return *dev != NULL;
	case RX: *dev = iio_context_find_device(ctx, "cf-ad9361-lpc");  return *dev != NULL;
	default:  return false;
	}
}

/* finds AD9361 streaming IIO channels */
static bool get_ad9361_stream_ch(struct iio_context *ctx, enum iodev d, struct iio_device *dev, int chid, struct iio_channel **chn)
{
	*chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
	if (!*chn)
		*chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
	return *chn != NULL;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
	switch (d) {
	case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), false); return *chn != NULL;
	case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), true);  return *chn != NULL;
	default:  return false;
	}
}

/* finds AD9361 local oscillator IIO configuration channels */
static bool get_lo_chan(struct iio_context *ctx, enum iodev d, struct iio_channel **chn)
{
	switch (d) {
	 // LO chan is always output, i.e. true
	case RX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 0), true); return *chn != NULL;
	case TX: *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("altvoltage", 1), true); return *chn != NULL;
	default:  return false;
	}
}

/* applies streaming configuration through IIO */
bool cfg_ad9361_streaming_ch(struct iio_context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
	struct iio_channel *chn = NULL;

	// Configure phy and lo channels
	printf("* Acquiring AD9361 phy channel %d\n", chid);
	if (!get_phy_chan(ctx, type, chid, &chn)) {	return false; }
	wr_ch_str(chn, "rf_port_select",     cfg->rfport);
	wr_ch_lli(chn, "rf_bandwidth",       cfg->bw_hz);
	wr_ch_lli(chn, "sampling_frequency", cfg->fs_hz);

	// Configure LO channel
	printf("* Acquiring AD9361 %s lo channel\n", type == TX ? "TX" : "RX");
	if (!get_lo_chan(ctx, type, &chn)) { return false; }
	wr_ch_lli(chn, "frequency", cfg->lo_hz);
	return true;
}

bool cfg_ad9361_streaming_ch_QRG(struct iio_context *ctx, long long qrg, enum iodev type)
{
	struct iio_channel *chn = NULL;

	if (!get_lo_chan(ctx, type, &chn)) 
        return false;
	wr_ch_lli(chn, "frequency", qrg);
	return true;
}

extern double lastsdrqrg;
void reset_Qrg_Pluto()
{
    lastsdrqrg = tuned_frequency;
    
    printf("re-tune: %.6f MHz\n",lastsdrqrg/1e6);
    
    if(cfg_ad9361_streaming_ch_QRG(ctx, (long long)tuned_frequency, RX) == false)
        printf("PLUTO: cannot set QRG\n");
}

void Pluto_setTunedQrgOffset(int hz)
{
    if(lastsdrqrg == 0) lastsdrqrg = tuned_frequency;
    
    double off = (double)hz;
    lastsdrqrg = lastsdrqrg - off;
    printf("set tuner: new:%f offset:%f\n",lastsdrqrg,off);
    
    if(cfg_ad9361_streaming_ch_QRG(ctx, (long long)lastsdrqrg, RX) == false)
        printf("PLUTO: cannot set QRG\n");
    
    printf("set tuner : %.6f MHz\n",lastsdrqrg/1e6);
}

int setup_pluto()
{
    // Streaming devices
	struct iio_device *rx;

	// Stream configurations
	struct stream_cfg rxcfg;

	// RX stream config
	rxcfg.bw_hz = 2000000; // 2 MHz rf bandwidth
	rxcfg.fs_hz = 3600000; // 3.8 MS/s rx sample rate
	rxcfg.lo_hz = (long long)tuned_frequency;
	rxcfg.rfport = "A_BALANCED"; // port A (select for rf freq.)

	printf("* Acquiring pluto's IIO context\n");
    ctx = iio_create_context_from_uri(pluto_context_name);
    if(!ctx) 
    {
        printf("FAILED: Acquiring pluto's IIO context\n");
        return 0;
    }
    
    unsigned int nr_devices = iio_context_get_devices_count(ctx);
    if(nr_devices == 0)
    {
        printf("no pluto devices found\n");
        return 0;
    }    
	
    bool bret = get_ad9361_stream_dev(ctx, RX, &rx);
    if(!bret)
    {
        printf("FAILED: no rx dev found\n");
        return 0;
    }    
    
    bret = cfg_ad9361_streaming_ch(ctx, &rxcfg, RX, 0);
	if(!bret)
    {
        printf("FAILED: RX port 0 not found\n");
        return 0;
    }

	printf("* Initializing AD9361 IIO streaming channels\n");
    bret = get_ad9361_stream_ch(ctx, RX, rx, 0, &rx0_i);
	if(!bret)
    {
        printf("FAILED: RX chan i not found\n");
        return 0;
    }
    
    bret = get_ad9361_stream_ch(ctx, RX, rx, 1, &rx0_q);
	if(!bret)
    {
        printf("FAILED: RX chan q not found\n");
        return 0;
    }

	iio_channel_enable(rx0_i);
	iio_channel_enable(rx0_q);

	rxbuf = iio_device_create_buffer(rx, PLUTO_RXBUFSIZE, false); // TODO, check best buffer size size
	if (!rxbuf) {
		printf("Could not create RX buffer\n");
		return 0;
	}
	return 1;
}

int pluto_create_rxthread()
{
    void *pluto_rxproc(void *pdata);
    pthread_t pluto_tid; 

    int ret = pthread_create(&pluto_tid,NULL,pluto_rxproc, NULL);
    if(ret)
    {
        printf("pluto RX process NOT started\n");
        return 0;
    }
    return 1;
}

void *pluto_rxproc(void *pdata)
{
ssize_t nbytes_rx;
char *p_dat, *p_end;
ptrdiff_t p_inc;
short ibuf[PLUTO_RXBUFSIZE];
short qbuf[PLUTO_RXBUFSIZE];
        
    pthread_detach(pthread_self());
    
    while(stop == false)
    {
        // Refill RX buffer
		nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0) 
            printf("PLUTO: Error refilling buf %d\n",(int) nbytes_rx); 
        
        // READ: Get pointers to RX buf and read IQ from RX buf port 0
        int dstlen = 0;
        int bnum = 0;
		p_inc = iio_buffer_step(rxbuf);
		p_end = iio_buffer_end(rxbuf);
		for (p_dat = (char *)iio_buffer_first(rxbuf, rx0_i); p_dat < p_end; p_dat += p_inc) 
        {
            if(++bnum & 1)  // decimate by 2
            { 
                int16_t i = ((int16_t*)p_dat)[0]; // Real (I)
                int16_t q = ((int16_t*)p_dat)[1]; // Imag (Q)
                
                ibuf[dstlen] = (short)i;
                qbuf[dstlen] = (short)q;
                dstlen++;
            }
		}
		fssb_sample_processing(ibuf, qbuf, dstlen);
    }
    
    printf("exit pluto RX thread\n");
    pthread_exit(NULL);
}
