# WebSDR Client specially for the es'hail-2 Satellite
a web based SDR program made for the SDRplay RSP1a

# made for LINUX ONLY ! 

a PC runs the SDR software:
* capturing samples from the SDR receiver (SDRplay)
* creating one line of a waterfall over a range of 200kHz
* downmixing a selected QRG into baseband
* creating a line of baseband waterfall over 20 kHz
* SSB demodulation and playing to a soundcard
* AUTO Beacon Lock

the waterfall data are written into the Apache HTML directory.

User Interface:
the GUI runs in a browser
* receiving the single line of the waterfalls via WebSocket
* drawing the full waterfall
* creating the GUI
* sending user command via WebSocket to above SDR program

# this is WORK in PROGRESS
actual Status: 
working with the playSDR
1) compile "make" and start "./playSDReshail2". The hardware is detected automatically if the original SDRplay driver is installed
2) copy the files from the html directory to your web server directory (i.e. /var/www/html)
   the web server must be running
3) open a web browser and open the html web site
4) if all is ok then the waterfall will be running. Click into the waterfall the select the listening frequency.
5) click the "Audio ON" button

Frequency Adjustment:
=====================
first un-select "Sync ON/off"
now you can correct the SDR tuner frequency manually in the text box below. You can also use the mouse-wheel to set it in 100Hz steps, or enter a value in Hz.
When the beacon is close to the correct position then select "Sync ON/off" and the software will automatically correct the LNB drift.

This works with the SDRplay hardware because it has a resolution of 1 Hz. It does not work with the RTLsdr sticks (the frequency will jump up and down) but you can give it a try.

RTL-SDR:
========
the rtl sdr hardware is automatically detected (librtlsdr must be installed). All works fine, except the auto-beacon-lock. This is work in progess.


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
