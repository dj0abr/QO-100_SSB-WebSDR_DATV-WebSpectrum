#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include "plutodrv.h"
#include "qo100websdr.h"
#include "wb_fft.h"
#include "ssbfft.h"
#include "setup.h"
#include <iio.h>  // libiio-dev must be installed
#include <arpa/inet.h>

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

// Streaming devices
struct iio_device *rx;

bool stop = false;
char tmpstr[64];
void *pluto_rxproc(void *pdata);
pthread_t pluto_tid; 

#define PLUTO_RXBUFSIZE 360000  // no of samples per buffer call

/*
 * we need 1.8MS sample rate
 * Pluto can only >2.5MS so we choose 3.8MS and decimate by 2
 */

char pluto_serialnumber[100] = {0};
// leave context name empty if serial number is used !
char pluto_context_name[50] = {""}; 

int init_pluto()
{
    // check if pluto_ip contains a valid IP or not
    struct sockaddr_in sa;
    
    int res = inet_pton(AF_INET,pluto_ip,&(sa.sin_addr));
    if(res == 1)
    {
        // we have a valid pluto IP continue using this IP
        sprintf(pluto_context_name,"ip:%s",pluto_ip);
        printf("search PLUTO at IP: <%s>\n",pluto_context_name);
        if(setup_pluto() == 1)
        {
            if(pluto_create_rxthread() == 1)
            {
                printf("PLUTO initialized: OK\n");
                return 1;
            }
        }
        printf("PLUTO not found at IP: %s\n",pluto_context_name);
        printf("try to find a PLUTO via USB\n");
    }
    
    // continue with USB
    printf("search PLUTO at USB\n");
    /*
    if(strlen(pluto_context_name) >= 7)
    {
        // use specific default pluto
        printf("use default PLUTO: <%s>\n",pluto_context_name);
        if(setup_pluto() == 1)
        {
            if(pluto_create_rxthread() == 1)
            {
                printf("PLUTO initialized: OK\n");
                return 1;
            }
        }
        printf("connot initialize this pluto\n");
        return 0;
    }
    */
    char s[500];
    snprintf(s,499,"iio_info -s 2>/dev/null");
    s[499] = 0;
    FILE *fp = popen(s,"r");
    if(fp)
    {
        while (fgets(s, sizeof(s)-1, fp) != NULL) 
        {
            // get the USB id
            char usbid[50];
            char usbsn[100];

            char *hp = strstr(s,"[usb:");
            if(hp)
            {
                hp += 1;
                char *he = strchr(hp,']');
                if(he)
                {
                    *he = 0;
                    strncpy(usbid,hp,49);
                    usbid[sizeof(usbid)-1] = 0;

                    // read serial number
                    char *psn = strstr(s,"serial=");
                    if(psn)
                    { 
                        psn+=7;
                        char *spn = strchr(psn,' ');
                        if(spn)
                        {
                            *spn = 0;
                            strncpy(usbsn,psn,99);
                            usbsn[sizeof(usbsn)-1] = 0;
                            printf("PLUTO found, SN:%s ID:%s\n",usbsn,usbid);

                            // if no special pluto requested, then use the first found pluto
                            // or search for a specific SN
                            if(*pluto_serialnumber == 0 || !strcmp(pluto_serialnumber, usbsn))
                            {
                                strcpy(pluto_context_name, usbid);
                                printf("use PLUTO: <%s>\n",pluto_context_name);
                                if(setup_pluto() == 1)
                                {
                                    if(pluto_create_rxthread() == 1)
                                    {
                                        printf("PLUTO initialized: OK\n");
                                        return 1;
                                    }
                                }
                                printf("connot initialize this pluto\n");
                                return 0;
                            }
                        }
                    }
                }
            }
        }
        pclose(fp);
    }
    else
        printf("cannot execute iio_info command\n");
    
    printf("no PLUTO found\n");
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

/* write attribute: double */
static void wr_ch_double(struct iio_channel *chn, const char* what, double str)
{
	errchk(iio_channel_attr_write_double(chn, what, str), what);
}

/* helper function generating channel names */
static char* get_ch_name(const char* type, int id)
{
	snprintf(tmpstr, sizeof(tmpstr), "%s%d", type, id);
    printf("name <%s>\n",tmpstr);
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
    printf("get_ad9361_stream_ch\n");
	*chn = iio_device_find_channel(dev, get_ch_name("voltage", chid), d == TX);
	if (!*chn)
		*chn = iio_device_find_channel(dev, get_ch_name("altvoltage", chid), d == TX);
	return *chn != NULL;
}

/* finds AD9361 phy IIO configuration channel with id chid */
static bool get_phy_chan(struct iio_context *ctx, enum iodev d, int chid, struct iio_channel **chn)
{
    printf("get_phy_chan\n");
	switch (d) {
	case RX:printf("RX <%s>\n",get_ch_name("voltage", chid));
            *chn = iio_device_find_channel(get_ad9361_phy(ctx), get_ch_name("voltage", chid), false); 
            return *chn != NULL;
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
    printf("cfg_ad9361_streaming_ch type:%d chid:%d\n",type,chid);
    
	struct iio_channel *chn = NULL;

	// Configure phy and lo channels
	//printf("* Acquiring AD9361 phy channel %d\n", chid);
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

#define REFTXPWR 10
bool cfg_ad9361_streaming_ch_TXpower(struct iio_context *ctx, struct stream_cfg *cfg, enum iodev type, int chid)
{
	struct iio_channel *chn = NULL;

	if (!get_phy_chan(ctx, type, chid, &chn)) {	return false; }
    double dBm = 0; // output power
    wr_ch_double(chn, "hardwaregain",dBm - REFTXPWR);
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
	// Stream configurations
	struct stream_cfg rxcfg;

    // === band width and set sample rate ===
    // RX
    #ifndef WIDEBAND
        // NB Transponder RX stream config
        rxcfg.bw_hz = 2000000; // 2 MHz rf bandwidth
        printf("set Pluto NB RX Sample Rate: %d\n",SDR_SAMPLE_RATE);
        rxcfg.fs_hz = SDR_SAMPLE_RATE; // 3.6 MS/s rx sample rate
    #else
        // WB Transponder RX stream config
        rxcfg.bw_hz = 20000000; // 20 MHz rf bandwidth
        rxcfg.fs_hz = SDR_SAMPLE_RATE; // 10 MS/s rx sample rate
    #endif

    // === frequency and port ===
    // RX
	rxcfg.lo_hz = (long long)tuned_frequency;
	rxcfg.rfport = "A_BALANCED"; // port A (select for rf freq.)

	//printf("* Acquiring pluto's IIO context\n");
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
	
    // === get AD9361 streaming devices ===
    // RX
    printf("get_ad9361_stream_dev RX\n");
    bool bret = get_ad9361_stream_dev(ctx, RX, &rx);
    if(!bret)
    {
        printf("FAILED: no rx dev found\n");
        return 0;
    }    

    // === configure AD9361 for streaming ===
    // RX
    printf("cfg_ad9361_streaming_ch RX 0\n");
    bret = cfg_ad9361_streaming_ch(ctx, &rxcfg, RX, 0);
	if(!bret)
    {
        printf("FAILED: RX port 0 not found\n");
        return 0;
    }

	// === Initializing AD9361 IIO streaming channels ===
    // RX I
    printf("get_ad9361_stream_ch RX 0\n");
    bret = get_ad9361_stream_ch(ctx, RX, rx, 0, &rx0_i);
	if(!bret)
    {
        printf("FAILED: RX chan i not found\n");
        return 0;
    }
    // RX Q
    printf("get_ad9361_stream_ch RX 1\n");
    bret = get_ad9361_stream_ch(ctx, RX, rx, 1, &rx0_q);
	if(!bret)
    {
        printf("FAILED: RX chan q not found\n");
        return 0;
    }

    // === enable IIO streaming channels ===
    // RX
	iio_channel_enable(rx0_i);
	iio_channel_enable(rx0_q);

    // === create IIO buffers ===
    // RX
	rxbuf = iio_device_create_buffer(rx, PLUTO_RXBUFSIZE, false); // TODO, check best buffer size
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
        printf("pluto process NOT started\n");
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

    printf("pluto process started\n");
    
    while(stop == false)
    {
        // === RX from Pluto ===
        // Refill RX buffer
		nbytes_rx = iio_buffer_refill(rxbuf);
		if (nbytes_rx < 0) 
            printf("PLUTO: Error refilling buf %d\n",(int) nbytes_rx); 
        
        // READ: Get pointers to RX buf and read IQ from RX buf port 0
        int dstlen = 0;
        #ifndef WIDEBAND
        int bnum = 0;
        #endif
		p_inc = iio_buffer_step(rxbuf);
		p_end = iio_buffer_end(rxbuf);
		for (p_dat = (char *)iio_buffer_first(rxbuf, rx0_i); p_dat < p_end; p_dat += p_inc) 
        {
            #ifndef WIDEBAND
            if(++bnum & 1)  // decimate by 2, only for NB transponder
            #endif
            { 
                int16_t i = ((int16_t*)p_dat)[0]; // Real (I)
                int16_t q = ((int16_t*)p_dat)[1]; // Imag (Q)
                
                ibuf[dstlen] = (short)i;
                qbuf[dstlen] = (short)q;
                dstlen++;
            }
		}
        
        #ifdef WIDEBAND
            wb_sample_processing(ibuf, qbuf, dstlen);
        #else
            fssb_sample_processing(ibuf, qbuf, dstlen);
        #endif
    }
    
    printf("exit pluto thread\n");
    pthread_exit(NULL);
}
