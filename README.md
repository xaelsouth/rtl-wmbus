# rtl-wmbus: software defined receiver for Wireless-M-Bus with RTL-SDR

rtl-wmbus is a software defined receiver for Wireless-M-Bus. It is written in plain C and uses RTL-SDR (https://github.com/osmocom/rtl-sdr) to interface with RTL2832-based hardware.

It can also use LimeSDR or other SDR receiver as backend through rx_sdr (see https://github.com/rxseger/rx_tools), which in turn is using SoapySDR to interface with underlying hardware in a vendor-neutral way.

Wireless-M-Bus is the wireless version of M-Bus ("Meter-Bus", http://www.m-bus.com), which is an European standard for remote reading of smart meters.

The primary purpose of rtl-wmbus is experimenting with digital signal processing and software radio. rtl-wmbus can be used on resource constrained devices such Raspberry Pi Zero or Raspberry PI B+ overclocked to 1GHz. Any Android based tablet will do the same too.

rtl-wmbus provides:
 * filtering
 * FSK demodulation
 * dc offset removing
 * clock recovering
 * mode T1 and mode C1 packet decoding
 * mode S1 packet decoding

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

 * make debug # (no optimization, with all debug options on)

 * make release # (-O3 optimized version, without any debugging options)

 * make pi1 # (Raspberry Pi optimized version, without any debugging options, will build on RasPi1) only

Before building Android version the SDK and NDK have to be installed. See androidbuild.bat for how to build and install.

For Windows users:
 * Download and install https://downloads.myriadrf.org/builds/PothosSDR/PothosSDR-2021.02.28-vc16-x64.exe
 * Compile rtl-wmbus with MSYS2-MinGW just by executing make in MSYS2-MinGW-Console...
 * Or use precompiled binary from the build directory
 * Start in the console: "c:\Program Files\PothosSDR\bin\rtl_sdr" -f 868.95M -s 1600000 - 2>NUL | build\rtl_wmbus_x64.exe
 * Optionally replace the path to rtl_sdr with that where you have PothosSDR installed

   Usage
   -----
To save an I/Q-stream on disk and decode that off-line:
 * rtl_sdr samples.cu8 -f 868.95M -s 1600000
 * cat samples.cu8 | build/rtl_wmbus

To save an I/Q-stream and decode this immediately to see what's going on right now:
 * rtl_sdr -f 868.95M -s 1600000 - 2>/dev/null | tee samples.cu8 | build/rtl_wmbus

To run continuously without saving anything on disk:
 * rtl_sdr -f 868.95M -s 1600000 - 2>/dev/null | build/rtl_wmbus

To run continuously with rx_sdr (or even with a higher sampling and decimation rate)
 * rx_sdr -f 868.95M -s 1600000 - 2>/dev/null | build/rtl_wmbus
 * rx_sdr -f 868.95M -s 4000000 - 2>/dev/null | build/rtl_wmbus -d 5

Notice "-d 5" in the last line: it's a multiple of 800kHz resulting from the sample rate of 4MHz.

To count "good" (no 3 out of 6 errors, no checksum errors) packets:
 * cat samples.cu8 | build/rtl_wmbus 2>/dev/null | grep "[T,C,S]1;1;1" | wc -l

Carrier-frequency given at "-f" must be set properly. With my DVB-T-Receiver I had to choose carrier 50kHz under the standard of 868.95MHz. Sample rate at 1.6Ms/s should be used or use a multiple of 800kHz. RTL-SDR supports sampling rate up to 3.2 MSamples, so you can choose 1.6 MSamples, 2.4 MSamples or 3.2 MSamples.

See samples/rtlsdr_868.950M_1M6_samples2.cu8 for an example of two T1 mode devices.

On Android the driver must be started first with options given above. I/Q-data goes to a port which is to be set in the driver settings. Use get_net to get I/Q-data into rtl_wmbus.

The output data is semicolon separated and the meaning of the columns are:

`MODE;CRC_OK;3OUTOF6OK;TIMESTAMP;PACKET_RSSI;CURRENT_RSSI;LINK_LAYER_IDENT_NO;DATAGRAM_WITHOUT_CRC_BYTES.`

3OUTOF6OK is only relevant for T1 and can be ignored for C1 and S1 (always set to 1).

   Bugfixing
   -----
Mode C1 datagram type B is supported now - thanks to Fredrik Öhrström for spotting this and for providing raw datagram samples. An another thanks goes to Kjell Braden (afflux) and to carlos62 for the idea how to implement this.

Redefining CFLAGS and OUTPUT directory is allowed now (patch sent by dwrobel).

L(ength) field from C1 mode B datagrams does not include CRC bytes anymore: L field will now be printed as if the datagram would be received from a T1 or C1 mode A meter.

Significantly improved C1 receiver quality. Sad, but in the low-pass-filter was a bug: the stopband edge frequency was specified as 10kHz instead of 110kHz. I have changed the latter to 160kHz and recalculated filter coefficients.

Packet rssi value fixed for S1 mode (was always 0): thanks to alalons.

   Improvements
   -----
A new method for picking datagrams out of the bit stream that _could_ probably better perform in C1 mode has been implemented.
I called that "run length algorithm". I don't know if any similar term already exists in the literature.
The new method is very sensitive to bit glitches, which have to be filtered out of the bit stream with an asymmetrical deglitch filter.
The deglitch filter _must_ be implemented asymmetrical in this case, because RTL-SDR produces more "0" bits than "1" bits on it's output.
The latter seems more to be a hardware feature rather than a bug.

Run length algorithm is running in parallel (and is fully independant) to the time2 method. You will eventually get two
identical datagrams, where each has been decoded by its own methods. If you really want avoiding duplicates, then start
rtl_wmbus with "-r 0" or "-t 0" argument to prevent executing of run length or time2 method respectively.
You can play with arguments and check which method performs better for you. Please note, that both methods are active by default.

The advantage of the run length algorithm is that it works without IIR filter (which are needed only for clock recovery by time2 method). Therefore the run length algorithm can be applied to all RF ICs providing raw _demodulated_ signal without clock in the "transparent serial mode" like TI CC1125 (swru295e.pdf, 8.7.2) does.

An additional method introduces more calculation steps, so I'm not sure if Raspberry Pi 1 will still work with that.

Run length algorithm works well with a few mode C1 devices I had around me, but can still be improved with your help.

S1 mode datagrams can now be received! You have to start rtl_wmbus at 868.3MHz with
 * rtl_sdr -f 868.3M -s 1600000 - 2>/dev/null | build/rtl_wmbus

Last but not least, you can try to receive all datagrams (S1, T1, C1) _simultaneously_:
 * rtl_sdr -f 868.625M -s 1600000 - 2>/dev/null | build/rtl_wmbus -s

Notice in the last line:
 * "-s": which is needed to inform rtl_wmbus about required frequency translation
 * "868.625M": the new frequency to receive at

rtl_wmbus will then shift all frequencies
 * by +325kHz to new center frequency at 868.95Mhz (T1 and C1)
 * by -325kHz to new center frequency at 868.3Mhz (S1)

I have tested this so far and can confirm that it works for T1/C1 and S1. Thanks to alalons for providing me with bitstreams!

Optimization on frequencies translation by rearranging compute steps implemented as proposed by alalons.

Alalons (have I thanked you already?!) proposed a speed optimized arctan function. Performance gain is notable (factor ~2) but could reduce sensitivity slightly. I have seen that on receiving C1 mode datagrams - that's why the speed optimized version is not in use by default. A speed optimized arctan version can be activated by "-a" in the program options.

  Update 2026-Oct-31
  ------------------
Compile-time algorithm defaults are centralized in `rtlwmbus_settings.h`.
Each setting is guarded by `#ifndef`, so compiler `-D` overrides remain supported.
`ATAN2_APPROX_ENABLED=1` is the default and selects `atan2_approximation2()` for
both polar discriminators. The runtime `-a` option still selects the separate
imaginary-only discriminator and bypasses this setting. The atan approximation is
now enabled by default!

The T1/C1 and S1 post-demodulation FIR filters use mirrored history buffers.
This replaces two variable-length dot-product loops at each ring wrap with one
contiguous loop, allowing compiler vectorization without changing coefficient
order, floating-point accumulation order, or receiver settings. This however,
requires additional storage. Other FIR users, including adaptive filters,
retain their original history layout.

Optional SIMD FIR uses unaligned loads, reverses each four-sample history group,
and handles remaining taps with a scalar tail. It requires no extra history
storage. The backend is selected at compile time:
* `__SSE__` selects SSE, otherwise
* `__ARM_NEON` selects NEON, otherwise
* the implementation is scalar.

SSE is baseline on x86-64; NEON is available on standard ARM64 Linux targets.
`FIR_SIMD=1` is enabled by default; use `-DFIR_SIMD=0` to disable it.
No global fast-math flags are required. SIMD uses four partial sums, so its
output is not bitwise identical to the scalar FIR. The NEON implementation
reverses each four-sample history group and uses the same four-accumulator
reduction order as SSE. SIMD FIR and transposed IIR remain opt-in.

The pre-decimation moving averages update integer sums and history for every
input sample, but convert to float and normalize only for retained samples.
The 8/16-sample windows and float-to-integer input truncation are unchanged.

The clock-recovery filters retain their original direct-form II realization by
default. An alternative transposed direct-form II implementation uses two delay
values per biquad and a different feedback schedule. It is not bitwise equivalent:
floating-point rounding can change clock zero crossings on untested signals.

DC offset removal is enabled by default and can be switched off with "--no-dc-offset".
For T1/C1 the offset is measured over the 16 symbols of the sync word (0x543D, balanced: 8 ones,
8 zeros) when it is detected and subtracted from the rest of the packet (time2 algorithm only;
the run length algorithm is unchanged). This replaces the former "-o" option, a continuously
running one-pole high-pass filter, which cost packets on most recordings. S1 uses the
same method over its 24 symbol sync word (0x547696, also balanced).

**Soft decision 3 out of 6 decoding (T1), `-c 0|1`.** The post-filter discriminator value of
each chip is kept and every data nibble is decoded by correlating its 6 chips with the 16 valid
3 out of 6 code words, instead of slicing each chip to a bit first. The L field is still
decoded hard because it is needed to tell T1 from C1. The run length algorithm stays hard.
Enabled by default; `-c 0` selects the old hard decision. With soft decoding a chip pattern
that is not a valid code word is mapped to the nearest code word, so the 3OUTOF6OK column is
always 1 for those symbols.

**Gaussian matched filter has not been kept.** Replacing the Butterworth post-filter by a
Gaussian FIR (BT 0.3 to 1.0) was worse at every BT. The best was BT 0.7 to 0.8 for the existing
filter.

**DC offset removal, `--no-dc-offset` to disable.** The former `-o` (one-pole high-pass, alpha 0.999,
about 1.25 ms time constant) tracked noise between packets and its step response was still decaying
during the sync word and the first part of the payload. It is replaced by an estimate from the last
16 sampling instants (the mean of the discriminator values), which is held from the moment the sync
word is detected until the packet ends.

The same is done for S1 (24 symbols). S1's run length algorithm still gets the old continuous high-pass,
because without it the only S1 recording (2 packets) loses one; the T1/C1 run length algorithm gets none.
Both are controlled by the same switch.

  License
  -------

Copyright (c) 2015-2026 <xael.south@yandex.com>

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
