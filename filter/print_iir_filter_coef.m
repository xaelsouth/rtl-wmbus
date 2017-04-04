function [] = print_iir_filter_coef(b, a);

[s, g] = tf2sos(b, a);

[R, C] = size(s);

fprintf(stdout, "#define GAIN %.10g\n", g);
fprintf(stdout, "#define SECTIONS %u\n", R);

fprintf(stdout, "static const float b[3*SECTIONS] = {");
for r = 1:R,
    for c = 1:C/2,
        fprintf(stdout, "%.10g, ", s(r,c));
    end
end
fprintf(stdout, "};\n");

fprintf(stdout, "static const float a[3*SECTIONS] = {");
for r = 1:R,
    for c = C/2+1:C,
        fprintf(stdout, "%.10g, ", s(r,c));
    end
end
fprintf(stdout, "};\n");

fprintf(stdout, "#undef SECTIONS\n");
fprintf(stdout, "#undef GAIN\n");

