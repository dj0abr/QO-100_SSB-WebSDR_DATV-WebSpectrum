# WebSDR Server for the QO-100 es'hail-2 Satellite
a web based SDR program made for the SDRplay RSP1a and RTL-SDR
uses any Browser in your network or Internet to view / listen your SDR data
  
# Server made for LINUX, GUI on any browser on any system.

NEW ! Supports NB and WB transponder !

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
for NB-transponder: 
any PC or SBC (Raspberry, Odroid..) running Ubuntu (or compatible like Mint ...)

for WB-transponder:
due to the high sampling rate of 10MS/s you need a PC (Intel/AMD) or
a fast SBC like the Odroid-N2, Odroid XU4 or Raspberry PI 4.

This software can be started twice (for NB and WB) on the same computer.

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

1) a couple of libraries are needed. These libraries are installed if you execute ./prepare_ubuntu in a terminal.
(php ... sometimes you need to replaced php with php5 or php7 depending on your linux distribution)

2) for SDRplay only: the SDRplay driver from the SDRplay Webpage must be installed. Look for the driver that supports your computer.

3) copy this software in any empty directory. Keep the names and contents of the subfolders.

4) there are three scripts that build the software automatically:

build_RTLSDR ... build the software for the NB-Transponder and for the RTLsdr ONLY

build_SDRplay ... build the software for the NB-Transponder and the SDRplay and/or RTLsdr (requires the SDRplay libraries, see above)

build_SDRplay_WB ... build the software for the SDRplay and the Wideband (DATV) transponder

simply run one of these scripts in a terminal i.e.:  ./build_RTLSDR
when the script finishes without errors then the job is done and you can use the software.  

# starting the server
in the termimal enter:
sudo  ./qo100websdr  -f  frequency

('sudo' is only required for the very first start since it has to configure your webserver. Then you can use it without sudo).

frequency ... this is the frequency where the QO-100 signal is delivered by your LNB.

NB-Transponder: an unmodified LNB will deliver the signal on 739525000 Hz. This frequency must be the frequency of the CW Beacon minus 25 kHz.

WB-Transponder: an unmodified LNB will deliver the signal on 745000000 Hz. This frequency must be the middle of the transponder (10495 MHz).

If you use a down-mixer or if you feed the LNB's LO with a different frequency, then calculate the frequency as follows:

frequency (NB) = 10489525000 - LO of the LNB (Example: 10489525000 - 9750000000 = 739525000 Hz)

frequency (WB) = 10495000000 - LO of the LNB (Example: 10495000000 - 9750000000 = 745000000 Hz)

# secure start
for any reason (i.e. hardware or power failure) the server software may exit or crash sometimes.
This situation should be handled automatically. Therefore there are two short scripts which start the server automatically after an error.

es ... start the NB narrow band SSB version
wb ... start the WB wide band DATV version

instead of starting the software with: ./qo100websdr  -f  frequency

you better start it with:  ./es   or  ./wb

Before doing this you will need to enter the correct frequencies delivered by your LNB into these script !!!

# using the web spectrum and waterfall monitor
open a browser (i.e. firefox, chrome...) and enter the IP address of your computer running the server followd by /nb (Narrowband transponder) or /wb (DATV transponder)

Example: 
your computer runs the WB (DATV wideband) version and has the IP address:  192.168.0.55
to view the DATV transponder enter in your browser:  192.168.0.55/wb

or if you have compiled this software for the narrow band transponder, then enter in
your browser: 192.168.0.55/nb

# access the website from outside (from the internet)
in your internet router you need to open three TCP ports for external access:
1) the port to your webserver: 80
2) the port to the websocket: 8090 and 8091

# running the NB and WB version of this software simultaneously
If you want to run the WB monitor with an SDRplay RSPx and also the NB monitor then you can do that simultaneously.
In this case the RSPx SDR must be used for the WB wide band monitor
and for the NB monitor you need to use an RTL-SDR.

Make two directories, one for WB and the other for NB and copy this software into both.
In one directory compile the software for WB by running  ./build_SDRplay_WB
and in the other directory run  ./build_RTLSDR

you can now start the WB version and the NB version simultaneously. Just be sure that you use the correct frequencies for each version.

The CPU load is quite high if you run both, but is not a problem for a modern PC, and also an Odroid N2 can handle both.

# ICOM synchronisation (for NB SSB transponder)
an ICOM TRX can be connected via CIV (USB) to the computer running this software.
When the ICOM checkbox is activated in the browser, the frequency of the Icom TRX will be read and the marker and lower waterfall will be set to this frequency every 100ms.
To adapt the CIV address to your Icom Transceiver, open file: civ.c and change variable civ_adr to the address of the transceiver. Then rebuild the software.

vy 73
de DJ0ABR, Kurt Moraw
Germany
