#define BUFFER_ELEMENT_SIZE (50000)
#define BUFFER_LENGTH 50

void initpipe();
void removepipe();
char write_pipe(int pipenum, unsigned char *data, int len);
int read_pipe_wait(int pipenum, unsigned char *data, int maxlen);
int read_pipe(int pipenum, unsigned char *data, int maxlen);
int NumberOfElementsInPipe(int pipenum);

enum {
    FIFO_RTL = 0,
    FIFO_AUDIO,
    FIFO_AUDIOWEBSOCKET,
    NUM_OF_PIPES
};
