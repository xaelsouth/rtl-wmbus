# rtl-wmbus: software defined receiver for Wireless-M-Bus with RTL-SDR

rtl-wmbus is a software defined receiver for Wireless-M-Bus. It is written in plain C and uses RTL-SDR (https://github.com/osmocom/rtl-sdr) to interface with RTL2832-based hardware.

Wireless-M-Bus is the wireless version of M-Bus ("Meter-Bus", http://www.m-bus.com), which is an European standard for remote reading of smart meters.

The primary purpose of rtl-wmbus is experimenting with digital signal processing and software radio. rtl-wmbus can be used on resource constrained devices such Raspberry Pi Zero or Raspberry PI B+ overclocked to 1GHz. Any Android based tablet will do the same too.

rtl-wmbus provides:
 * filtering
 * FSK demodulating
 * clock recovering
 * mode T1 and mode C1 packet decoding

rtl-wmbus requires:
 * Linux or Android
 * C99
 * supported DVB-T receiver
 * RTL-SDR library for Linux: http://sdr.osmocom.org/trac/wiki/rtl-sdr or
 * (optional, only for android builds) RTL-SDR library port for Android: https://github.com/martinmarinov/rtl_tcp_andro-

For the latest version, see https://github.com/xaelsouth/rtl-wmbus


  Installing
  ----------

The Osmocom RTL-SDR library must be installed before you can build rtl-wmbus. See http://sdr.osmocom.org/trac/wiki/rtl-sdr for more
information. RTL-SDR library for Android would be installed via Google Play.

To install rtl-wmbus, download, unpack the source code and go to the top level directory. Then use one of these three options:

 * make debug # (no optimization at all, with debug options)

 * make release # (-O3 optimized version, without any debugging options)

 * make pi1 # (Raspberry Pi optimized version, without any debugging options, will build on RasPi1) only

Before building Android version the SDK and NDK have to be installed. See androidbuild.bat for how to build.

   Usage
   -----
 To save an IQ-stream on disk and decode that off-line:

 * rtl_sdr samples.bin -f 868.95M -s 1600000
 * cat samples.bin | build/rtl_wmbus

 To run continuously:

 * rtl_sdr -f 868.95M -s 1600000 - 2>/dev/null | build/rtl_wmbus

 To count "good" (no 3 out of 6 errors, no checksum errors) packets:

 * cat samples.bin | build/rtl_wmbus 2>/dev/null | grep "[T,C]1;1;1" | wc -l

Carrier-frequency given at "-f" must be set properly. With my DVB-T-Receiver I had to choose carrier 50kHz under the standard of 868.95MHz. Sample rate at 1.6Ms/s is hardcoded and cannot be changed.

samples2.bin is a "live" example with two devices received.

On Android first the driver must be started with options given above. IQ-data goes to a port which is would be already set by driver settings. Use get_net to get IQ-data into rtl_wmbus.

   Bugfixing
   -----
Mode C1 datagram type B is supported now - thanks to Fredrik Öhrström for spotting this bug and for providing raw datagram samples.
An another thanks to Kjell Braden (afflux) and to carlos62 for the idea how to fix this.
   
  License
  -------

Copyright (c) 2017 <xael.south@yandex.com>

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
