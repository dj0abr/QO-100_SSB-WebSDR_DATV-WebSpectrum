#include <unistd.h>
#include <stdint.h>
#include "../qo100websdr.h"

#define MESSAGE_LENGTH 30000
#define MAX_CLIENTS    20   // if changed: change also fifo.h !!!!!!!!!

#define WS_KEY_LEN     24
#define WS_MS_LEN      36
#define WS_KEYMS_LEN   (WS_KEY_LEN + WS_MS_LEN)
#define MAGIC_STRING   "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

#define WS_HS_REQ      "Sec-WebSocket-Key"

#define WS_HS_ACCLEN   130
#define WS_HS_ACCEPT                   \
"HTTP/1.1 101 Switching Protocols\r\n" \
"Upgrade: websocket\r\n"               \
"Connection: Upgrade\r\n"              \
"Sec-WebSocket-Accept: "               \

/* Frame definitions. */
#define WS_FIN    128

/* Frame types. */
#define WS_FR_OP_TXT  1
#define WS_FR_OP_BINARY 2
#define WS_FR_OP_CLSE 8

#define WS_FR_OP_UNSUPPORTED 0xF

#ifdef WIDEBAND
    #define AUDIO_RATE 1 // dummy, not used for WB mode
#endif

// list of sockets, -1=inactive
typedef struct {
    int socket;     // socket id
    unsigned char msg0[MESSAGE_LENGTH];  // big waterfall message to send to the browser
    unsigned char msg1[MESSAGE_LENGTH];  // small waterfall message to send to the browser
    int msglen0;
    int msglen1;
    int send0;       // 0=nothing to send, 1=send now
    int send1;
    unsigned char samples[AUDIO_RATE*2];
    int sendaudio;
} WS_SOCK;

// Events
struct ws_events
{
	/* void onopen(int fd); */
	void (*onopen)(int);
	
	/* void onclose(int fd); */
	void (*onclose)(int);

	/* void onmessage(int fd, unsigned char *message); */
	void (*onmessage)(int, unsigned char *);
    
    /* int onwork(int fd); do something short, worker function, called by the thread's main loop */
    int (*onwork)(int fd, unsigned char *cnt0, unsigned char *cnt1);
};

typedef struct {
    uint32_t command;
    uint32_t para;
    uint32_t client;
} USERMSG;

int getHSaccept(char *wsKey, unsigned char **dest);
int getHSresponse(char *hsrequest, char **hsresponse);

char* ws_getaddress(int fd);
int   ws_sendframe(int fd, char *msg);
int   ws_sendframe_binary(int fd, unsigned char *msg, uint64_t length);
int   ws_socket(struct ws_events *evs, int port);

void ws_init();
void ws_send(unsigned char *pwfdata, int idx, int wf_id, int client);
int get_useranz();
void onopen(int fd);
void onclose(int fd);
void onmessage(int fd, unsigned char *message);
int onwork(int fd, unsigned char *cnt0, unsigned char *cnt1);
void insert_socket(int fd, char *cli);
void remove_socket(int fd);
char *getSocketIP(int fd);
int test_socket(char *cli);
unsigned char *ws_build_txframe(int i, int *plength);
int get_socket_idx(int fd);
int isLocal(int idx);

extern WS_SOCK actsock[MAX_CLIENTS];
extern char myIP[20];
