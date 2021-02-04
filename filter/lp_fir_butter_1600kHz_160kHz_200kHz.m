clear all;

samplerate = 1600e3;
nyqistrate = samplerate/2;

Wp1 = 160e3/nyqistrate;
Ws1 = 200e3/nyqistrate;
Rp = 1;
Rs = 40;

[n, Wc] = buttord(Wp1, Ws1, Rp, Rs);
[h] = fir1(n, Wc);

f_delta = 50e3;
h_length = length(h);
h_new = h.*cos(2*pi*f_delta*[0:h_length-1]/samplerate);

print_fir_filter_coef(h);
print_fir_filter_coef(h_new);


