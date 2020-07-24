# WebSDR Server for the QO-100 es'hail-2 Satellite
a web based SDR program made for the Adalm-PLUTO, SDRplay RSP1a and RTL-SDR
uses any Browser in your network or Internet to view / listen your SDR data
  
# Server made for LINUX, GUI on any browser on any system.

Update V2.9:
- Adalm-PLUTO supported for NB Transponder
- Adalm-PLUTO supported for WB Transponder
Attention: run  ./prepare_ubuntu_pluto to install all required libraries

Update V2.81:
- fixed: Raspberry + RTLsdr did not set correct frequency

Update V2.8:
- beacon LOCK now working with the new bandplan

Update V2.73:
- adapted to new bandplan. Beaconlock temporarily deactivated. RTLsdr offset can be corrected in rtlsdr.c #define RTL_TUNER_CORRECTION. New beacon lock follows in the next update.

Update V2.72:
- choose transceiver (icom) type in the setup menu (Sat or TX only)
- control the transceiver by right mouse click into one of the waterfalls

Update V2.71:
- SETUP menu: sometimes the contents was not loaded, this is fixed now.

Update V2.7:
- NEW SETUP menu: DO NOT make ANY changes to qo100websdr.h ! All settings can now be done in the browser's SETUP menu.

Update V2.5:
- bug fix for 32bit machines (i.e.RPI). Wrong frequency on mouse click, fixed
- multi threading FFT reduces CPU-core load on RPI

Update V2.3:
- bug fixes in multi user mode

Update V2.2:
- new audio filter: now filtering is done in the browser, so every user has his own filters.

Update V2.1:
- new and better SSB demodulator using inverse FFT. Preconfigured for max. 20 independent users. Can be changed in webspcketserver.h, see MAX_CLIENTS. Maximum number of users is given by the CPU power of your computer. I.e.: an Odroid N2 or RPI-4 can handle up to 20 users, a PC can handle much more.

Update V2.0:
- wide band version supports Minitiouner Remote Control by clicking in the spectrum/waterfall. See documentation below

Update V1.9:
- first multi user version, each browser can hear its own frequency, max. 20 simultaneous users

Update V1.8:
- many fixes for 32bit computers like Raspberry PI
- new beacon lock algorithm

Update V1.7:
- fixed SSB demodulation

Update V1.6:
- prepared for the new upcoming NB bandplan
- new 500kHz segment in WB bandplan included
- automatically measures transponder noise (nice for comparing dishes and receivers)

Update V1.5:
- totally new frequency calculations. Now it's possible to configure the NB monitor by just modifying the values in qo100websdr.h
- frequency correction value in qo100websdr.h to compensate the bad crystals in most RTLsdr sticks.

# NB-Transponder
* Spectrum
* Big waterfall showing the complete transponder (300kHz)
* Small waterfall showing a selectable 15kHz segment
* Audio: listen to SSB signals via your browser
* AUTO Beacon Lock
* ICOM synchronisation of the transceiver to the waterfall
* for SDRplay RSPx and RTLsdr and compatibles

# WB (DATV) Transponder
* Spectrum
* waterfall showing the complete transponder (8MHz)
* for SDRplay RSPx only (since we need 10MS/s sampling rate)

# required computer
a PC (Intel/AMD) or
a fast SBC like the Odroid-N2, Odroid XU4 or Raspberry PI 4.

may also run on slower computers, not guaranteed, try it.

# General function of the server
* capturing samples from the SDR receiver
* creating one line of a waterfall
* sending this waterfall line via port 8090 (WB) or 8091 (NB) to the browser
* downmixing a selected QRG into baseband
* creating a line of baseband waterfall over 15 kHz
* SSB demodulation and playing to the browser
* AUTO Beacon Lock

# Function in the browser
* the User Interface (GUI) runs in a browser on a PC, Tablet, Smartphone ... on any computer in your local network or even in the internet
* receiving the single line of the waterfalls via WebSocket
* drawing the full waterfall and spectrum
* sending user command via WebSocket to above SDR program

# Installation

1) a couple of libraries are needed. These libraries are installed if you execute ./prepare_ubuntu or ./prepare_ubuntu_pluto in a terminal.
(php ... sometimes you need to replaced php with php5 or php7 depending on your linux distribution)

2) for SDRplay only: the SDRplay driver from the SDRplay Webpage must be installed. Look for the driver that supports your computer.

3) copy this software in any empty directory. Keep the names and contents of the subfolders.

4) optional: identify the serial port of an ICOM transceiver:

    serial-USB devices my change their ttyUSB number randomly. For an exact identification do the following:
    
    1) connect your USBserial adapter
    2) enter this command in a terminal:
       ls  /dev/ttyUSB*
       this shows the device number i.e.: /dev/ttyUSB0
    3) enter this command in a terminal, replace ttyUSB0 with the correct number
       udevadm info -a -p  $(udevadm info -q path -n /dev/ttyUSB0) | grep '{serial}' | cut -d \" -f2 | head -n 1
       this prints the ID of the serial USB adapter
    4) enter this ID into the file identifySerUSB.c in the line #define SERID "xxxxxx", where xxxxxx is the ID

5) there are three scripts that build the software automatically:

build_RTLSDR ... build the software for the NB-Transponder and for the RTLsdr ONLY

build_SDRplay ... build the software for the NB-Transponder and the SDRplay and/or RTLsdr (requires the SDRplay libraries, see above)

build_PLUTO ... build the software for the NB-Transponder and for the Adalm-Pluto and/or RTLsdr 

build_SDRplay_WB ... build the software for the SDRplay and the Wideband (DATV) transponder

build_PLUTO_WB ... build the software for the Pluto and the Wideband (DATV) transponder 

simply run one of these scripts in a terminal i.e.:  ./build_RTLSDR
when the script finishes without errors then the job is done and you can use the software.  

# starting the server
in the termimal enter:
sudo  ./qo100websdr

('sudo' is only required for the very first start since it has to configure your webserver. Then you can use it without sudo).

# secure start
for any reason (i.e. hardware or power failure) the server software may exit or crash sometimes.
This situation should be handled automatically. Therefore there are two short scripts which start the server automatically after an error.

es ... start the NB narrow band SSB version

wb ... start the WB wide band DATV version

instead of starting the software with: ./qo100websdr

you better start it with:  ./es   or  ./wb

Before doing this you will need to enter the correct frequencies delivered by your LNB into these script !!!

# using the web spectrum and waterfall monitor
open a browser (i.e. firefox, chrome...) and enter the IP address of your computer running the server followd by /nb.html (Narrowband transponder) or /wb.html (DATV transponder)

Example: 
your computer runs the WB (DATV wideband) version and has the IP address:  192.168.0.55
to view the DATV transponder enter in your browser:  192.168.0.55/wb.html

or if you have compiled this software for the narrow band transponder, then enter in
your browser: 192.168.0.55/nb.html

# access the website from outside (from the internet)
in your internet router you need to open two TCP ports for external access:
1) the port to your webserver: 80
2) the port to the websocket: 8091

the websocket port can be changed in the Setup (see Browser Window top/right)

# running the NB and WB version of this software simultaneously
If you want to run the WB monitor with an SDRplay RSPx and also the NB monitor then you can do that simultaneously.
In this case the RSPx SDR must be used for the WB wide band monitor
and for the NB monitor you need to use an RTL-SDR.

Make two directories, one for WB and the other for NB and copy this software into both.
In one directory compile the software for WB by running  ./build_SDRplay_WB
and in the other directory run  ./build_RTLSDR

you can now start the WB version and the NB version simultaneously. Just be sure that you use the correct frequencies for each version.

The CPU load is quite high if you run both, but is not a problem for a modern PC, and also an Odroid N2 can handle both.

Also, use two different websocket ports for NB and WB !

# Minitiouner Remote Control
F6DZP has implemented an UDP interface into his excellent Minitiouner software. This QO-100 WebSDR supports this feature. Just click on a signal in the spectrum or waterfall and the Minitiouner will be tuned to this signal.

Before compilation you can change these settings in qo100websdr.h:
MINITIOUNER_IP ... enter the IP address of the PC running Minitiouner
MINITIOUNER_LOCAL ... if you want to remote control Minitiouner via the internet, enter 0. The default value 1 allows remote control in the home network only.
    

# ICOM synchronisation (for NB SSB transponder)
an ICOM TRX can be connected via CIV (USB) to the computer running this software.
When the ICOM checkbox is activated in the browser, the frequency of the Icom TRX will be read and the marker and lower waterfall will be set to this frequency every 100ms.
To adapt the CIV address to your Icom Transceiver, open file: civ.c and change variable civ_adr to the address of the transceiver. Then rebuild the software.

For automatic detection of you ICOM transceiver you may check the in file: identifySerUSB.c the definition:
#define SERID "IC-9700 13001397 A"

vy 73
de DJ0ABR, Kurt Moraw
Germany
