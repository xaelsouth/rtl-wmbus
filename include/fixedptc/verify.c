#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <locale.h>

//#define FIXEDPT_WBITS 16
#include "fixedptc.h"

/* This test program verifies the fixedpt precision, comparing it to
 * float and double precision results. */

static const float pi_f = 3.14159265358979323846264338;
static const double pi_d = 3.14159265358979323846264338;
static const fixedpt pi_x = fixedpt_rconst(3.14159265358979323846264338);

static const float e_f = 2.71828182845904523536028747;
static const double e_d = 2.71828182845904523536028747;
static const fixedpt e_x = fixedpt_rconst(2.71828182845904523536028747);

void
verify_numbers()
{
	printf("pi as string:\t3.14159265358979323846264338\n");
	printf("pi as float:\t%0.6f\n", pi_f);
	printf("pi as double:\t%0.15lf\n", pi_d);
	printf("pi as fixedpt:\t%s\n", fixedpt_cstr(pi_x, -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(pi_x, -1)) - pi_d);
	printf("pi as fixedpt converted to float: %0.6f\n", fixedpt_tofloat(pi_x));

	printf("e as string:\t2.71828182845904523536028747\n");
	printf("e as float:\t%0.6f\n", e_f);
	printf("e as double:\t%0.15lf\n", e_d);
	printf("e as fixedpt:\t%s\n", fixedpt_cstr(e_x, -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(e_x, -1)) - e_d);
}

void
verify_trig()
{
	printf("sin(pi) as float:\t%0.6f\n", sinf(pi_f));
	printf("sin(pi) as double:\t%0.15lf\n", sin(pi_d));
	printf("sin(pi) as fixedpt:\t%s\n", fixedpt_cstr(fixedpt_sin(pi_x), -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(fixedpt_sin(pi_x), -1)) - sin(pi_d));

	printf("sin(e) as float:\t%0.6f\n", sinf(e_f));
	printf("sin(e) as double:\t%0.15lf\n", sin(e_d));
	printf("sin(e) as fixedpt:\t%s\n", fixedpt_cstr(fixedpt_sin(e_x), -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(fixedpt_sin(e_x), -1)) - sin(e_d));

	printf("tan(e) as float:\t%0.6f\n", tanf(e_f));
	printf("tan(e) as double:\t%0.15lf\n", tan(e_d));
	printf("tan(e) as fixedpt:\t%s\n", fixedpt_cstr(fixedpt_tan(e_x), -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(fixedpt_tan(e_x), -1)) - tan(e_d));
}

void
verify_powers()
{
	printf("pow(pi,3) as float:\t%0.6f\n", powf(pi_f, 3));
	printf("pow(pi,3) as double:\t%0.15f\n", pow(pi_d, 3));
	printf("pow(pi,3) as fixedpt:\t%s\n", fixedpt_cstr(fixedpt_pow(pi_x, fixedpt_rconst(3)), -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(fixedpt_pow(pi_x, fixedpt_rconst(3)), -1)) - pow(pi_d, 3));

	printf("exp(3) as float:\t%0.6f\n", expf(3));
	printf("exp(3) as double:\t%0.15f\n", expf(3));
	printf("exp(3) as fixedpt:\t%s\n", fixedpt_cstr(fixedpt_exp(fixedpt_rconst(3)), -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(fixedpt_exp(fixedpt_rconst(3)), -1)) - exp(3));

	printf("ln(e) as float:\t%0.6f\n", logf(e_f));
	printf("ln(e) as double:\t%0.15f\n", log(e_d));
	printf("ln(e) as fixedpt:\t%s\n", fixedpt_cstr(fixedpt_ln(e_x), -1));
	printf("  delta fixedpt-double:\t%0.10lf\n", atof(fixedpt_cstr(fixedpt_ln(e_x), -1)) - log(e_d));
}

int
main() 
{
	setlocale(LC_NUMERIC, "C");
	printf("fixedptc library version: %s\n", FIXEDPT_VCSID);
	printf("Using %d-bit precision, %d.%d format\n\n", FIXEDPT_BITS, FIXEDPT_WBITS, FIXEDPT_FBITS);

	verify_numbers();
	printf("\n");
	verify_trig();
	printf("\n");
	verify_powers();

	return (0);
}

