# WebSDR Client specially for the es'hail-2 Satellite
a web based SDR program made for the SDRplay RSP1a

# made for LINUX ONLY ! There is a "Windows free zone" around my shack :-)

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

# this is WORK in PROGRESS
actual Status: 
working with the playSDR (maybe later also with the RTLsdr stick and compatibles)
1) compile "make" and start "./playSDReshail2". The hardware is detected automatically if the original SDRplay driver is installed
2) copy the files from the html directory to your web server directory (i.e. /var/www/html)
   the web server must be running
3) open a web browser and open the html web site
4) the waterfall muss be running. Click into the waterfall the select the listening frequency.
5) click the "Audio ON" button


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

./playSDReshail2
