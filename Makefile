CFLAGS?=-O3 -Wall -I./websocket -DSDR_PLAY -std=gnu99 -DESHAIL2 -DWIDEBAND
LDLIBS+= -lpthread -lm -lmirsdrapi-rsp -lfftw3 -lsndfile -lasound -lrtlsdr
CC?=gcc
PROGNAME=qo100websdr
OBJ=qo100websdr.o sdrplay.o fir_table_calc.o wf_univ.o websocket/websocketserver.o websocket/ws_callbacks.o websocket/base64.o websocket/sha1.o websocket/ws.o websocket/handshake.o audio.o setqrg.o rtlsdr.o timing.o fifo.o ssbfft.o audio_bandpass.o hilbert90.o antialiasing.o cat.o  civ.o wb_fft.o minitiouner.o

all: qo100websdr

websocket/%.o: websocket/%c
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

qo100websdr: $(OBJ)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o websocket/*.o qo100websdr

