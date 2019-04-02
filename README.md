# PlaySDR-Webclient
a web based SDR program made for the SDRplay RSP1a

a PC runs the SDR software:
* capturing samples from the SDR receiver (SDRplay)
* creating one line of a waterfall over a range of 200kHz
* downmixing a selected QRG into baseband
* creating a line of baseband waterfall over 20 kHz
* SSB demodulation and playing to a soundcard

the waterfall data are written into the Apache HTML directory.

User Interface:
the GUI runs in a browser
* receiving the single line of the waterfalls via WebSocket
* drawing the full waterfall
* creating the GUI
* sending user command via WebSocket to above SDR program

currently I am working with the SDRplay Hardware only.
But I use 2.4Msamples, so later on this can be compatible for
i.e. RTL sticks and others.

# this is WORK in PROGRESS
actual Status: 
working with the playSDR and the RTLsdr stick (and compatibles)
1) Setup: playSDRweb.h
2) compile and start
3) copy the html file to your web server directory (i.e. /var/www/html)
   the web server must be running
3) open a web browser and open the html web site
4) the waterfall muss be running. Click into the big waterfall the select the listening frequency. The base frequency is set in playSDRweb.h
5) click the "Audio ON" button

Today Mar,17 2019, this program was used for the first time the listen to es'hail-2


Prerequisites:
==============
these libraries are required:

apt-get update

apt-get install libasound2-dev libfftw3-dev libgd3 libgd-dev apache2 sndfile-tools libsndfile1-dev php

additionally the SDRplay driver from the SDRplay Webpage must be installed.

Make the software:
==================

make

Run the software:
=================

./playSDRweb
