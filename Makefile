CFLAGS?=-O3 -Wall -I./websocket -DSDR_PLAY
LDLIBS+= -lpthread -lm -lmirsdrapi-rsp -lfftw3 -lsndfile -lasound -lgd -lz -ljpeg -lfreetype -lrtlsdr
CC?=gcc
PROGNAME=playSDReshail2
OBJ=playSDReshail2.o sdrplay.o fir_table_calc.o wf_univ.o color.o websocket/websocketserver.o websocket/ws_callbacks.o websocket/base64.o websocket/sha1.o websocket/ws.o websocket/handshake.o audio.o setqrg.o rtlsdr.o timing.o fifo.o ssbfft.o audio_bandpass.o

all: playSDReshail2

websocket/%.o: websocket/%c
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

playSDReshail2: $(OBJ)
	$(CC) -g -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f *.o websocket/*.o playSDReshail2 fssb_wisom

