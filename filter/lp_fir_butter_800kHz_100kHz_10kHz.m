clear all;

samplerate = 800e3;
nyqistrate = samplerate/2;

Wp1 = 100e3/nyqistrate;
Ws1 = 10e3/nyqistrate;
Rp = 1;
Rs = 40;

[n, Wc] = buttord(Wp1, Ws1, Rp, Rs);
[b] = fir1(n, Wc);

print_fir_filter_coef(b);

