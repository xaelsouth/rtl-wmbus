function [] = print_ppf_filter_coef(b, phase_channels);

b_poly = buffer(b, phase_channels);

[R, C] = size(b_poly);

fprintf(stdout, "#define PHASES %u\n", R);
fprintf(stdout, "#define COEFFS %u\n", C);

fprintf(stdout, "static const float b[PHASES][COEFFS] = {");
for r = 1:R,
    fprintf("\n\t{");
    for c = 1:C,
        fprintf(stdout, "%.10g, ", b_poly(r,c));
    end
    fprintf("},");
end
fprintf(stdout, "};\n");

fprintf(stdout, "#undef COEFFS\n");
fprintf(stdout, "#undef PHASES\n");
