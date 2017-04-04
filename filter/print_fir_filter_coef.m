function [] = print_fir_filter_coef(b);

[R, C] = size(b);

fprintf(stdout, "#define COEFFS %u\n", C);

fprintf(stdout, "static const float b[COEFFS] = {");
for c = 1:C,
    fprintf(stdout, "%.10g, ", b(c));
end
fprintf(stdout, "};\n");

fprintf(stdout, "#undef COEFFS\n");

