clear all;

Fs = 1600e3; % samplerate
t = 0:Fs-1; % seconds
t = t./Fs;
f = -Fs/2:Fs/2-1;

fid = fopen("../samples.bin");
samples = fread(fid, size=Fs*2, precision="uint8"); % *2: i,q - samples interleaved
fclose(fid);

samples = samples .- 127;
signal = samples(1:2:end) .+ samples(2:2:end) .* j;

Wp1 = 160e3/(Fs/2);
Ws1 = 200e3/(Fs/2);
Rp = 1;
Rs = 40;

[n, Wc] = buttord(Wp1, Ws1, Rp, Rs);
[b] = fir1(n, Wc);

b_notch = [1,-1];
a_notch = [1 , -0.98];
%freqz(b_notch, a_notch);
%signal = filtfilt(b_notch, a_notch, signal); % filter dc offset

filtered_signal = filter(b, 1, signal); % low-pass

conj_filtered_signal = [conj(filtered_signal(2:end)); 0];
demodulated_signal = arg(filtered_signal .* conj_filtered_signal)/pi;
demodulated_signal2 = demodulated_signal.^2;


fid = fopen("../demod.bin", "w");
samples = fwrite(fid, demodulated_signal./max(abs(demodulated_signal)).*32767, precision="int16");
fclose(fid);


%%%%%%%%%


Ws1 = 90e3/(Fs/2);
Wp1 = 98e3/(Fs/2);
Wp2 = 102e3/(Fs/2);
Ws2 = 110e3/(Fs/2);
Rp = 1;
Rs = 40;

[n, Wc] = cheb1ord([Wp1, Wp2], [Ws1, Ws2], Rp, Rs);
[b, a] = cheby1(n, Rp, Wc);

takt = filter(b,a,demodulated_signal2); % band-pass

fft_signal = fft(signal);
fft_filtered_signal = fft(filtered_signal);
fft_demodulated_signal = fft(demodulated_signal);
fft_demodulated_signal2 = fft(demodulated_signal2);
fft_takt = fft(takt);

u = abs(fftshift(fft_signal));
v = abs(fftshift(fft_filtered_signal));
w = abs(fftshift(fft_demodulated_signal));
x = abs(fftshift(fft_demodulated_signal2));
y = abs(fftshift(fft_takt));

plot(f, [u, v]);
title('Amplitude Spectrum')
xlabel('f (Hz)')
ylabel('|signal|')
grid on;

