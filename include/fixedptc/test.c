#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* This is a pure integer-only test program for fixedptc */

//#define FIXEDPT_WBITS 16


#include "fixedptc.h"

void
fixedpt_print(fixedpt A)
{
	char num[20];

	fixedpt_str(A, num, -2);
	puts(num);
}

int main() {

	fixedpt A, B, C;
	
	printf("fixedptc library version: %s\n", FIXEDPT_VCSID);
	printf("Using %d-bit precision, %d.%d format\n\n", FIXEDPT_BITS, FIXEDPT_WBITS, FIXEDPT_FBITS);

	printf("The most precise number: ");
	fixedpt_print(1);
	printf("The biggest number: ");
	fixedpt_print(0x7fffff00);
	printf("Here are some example numbers:\n");

	printf("Random number: ");
	fixedpt_print(fixedpt_rconst(143.125));
	printf("PI: ");
	fixedpt_print(FIXEDPT_PI);
	printf("e: ");
	fixedpt_print(FIXEDPT_E);
	puts("");

	A = fixedpt_rconst(2.5);
	B = fixedpt_fromint(3);

	fixedpt_print(A);
	puts("+");
	fixedpt_print(B);
	C = fixedpt_add(A, B);
	puts("=");
	fixedpt_print(C);
	puts("");

	fixedpt_print(A);
	puts("*");
	fixedpt_print(B);
	puts("=");
	C = fixedpt_mul(A, B);
	fixedpt_print(C);
	puts("");

	A = fixedpt_rconst(1);
	B = fixedpt_rconst(4);
	C = fixedpt_div(A, B);

	fixedpt_print(A);
	puts("/");
	fixedpt_print(B);
	puts("=");
	fixedpt_print(C);

	printf("exp(1)=");
	fixedpt_print(fixedpt_exp(FIXEDPT_ONE));

	puts("");
	puts("sqrt(pi)=");
	fixedpt_print(fixedpt_sqrt(FIXEDPT_PI));
	
	puts("");
	puts("sqrt(25)=");
	fixedpt_print(fixedpt_sqrt(fixedpt_rconst(25)));

	puts("");
	puts("sin(pi/2)=");
	fixedpt_print(fixedpt_sin(FIXEDPT_HALF_PI));

	puts("");
	puts("sin(3.5*pi)=");
	fixedpt_print(fixedpt_sin(fixedpt_mul(fixedpt_rconst(3.5), FIXEDPT_PI)));

	puts("");
	puts("4^3.5=");
	fixedpt_print(fixedpt_pow(fixedpt_rconst(4), fixedpt_rconst(3.5)));

	puts("");
	puts("4^0.5=");
	fixedpt_print(fixedpt_pow(fixedpt_rconst(4), fixedpt_rconst(0.5)));

	return (0);
}
