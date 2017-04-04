clear all;

samplerate = 1600e3;
nyqistrate = samplerate/2;

Wp1 = 160e3/nyqistrate;
Ws1 = 200e3/nyqistrate;
Rp = 1;
Rs = 40;

[n, Wc] = buttord(Wp1, Ws1, Rp, Rs);
[b] = fir1(n, Wc);

x = 1:22;
y = filter(b,1,x);
y = y(2:2:end);

phase_channels = 2;
b_poly = buffer(b, phase_channels);
y1 = filter(b_poly(2,:), 1, x(1:2:end));
y2 = filter(b_poly(1,:), 1, x(2:2:end));

y_poly = y1 + y2;

y_err = y - y_poly;

print_ppf_filter_coef(b, phase_channels);
