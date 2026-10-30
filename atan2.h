#ifndef ATAN2_H
#define ATAN2_H

#include <math.h>


static inline float atan2_libm(float complex y)
{
    return cargf(y) * (float)M_1_PI;
}


/** https://gist.github.com/volkansalma/2972237 */
static inline float atan2_approximation2(float complex s)
{
    float y = cimagf(s), x = crealf(s);

    if (x == 0.0f)
    {
        if (y > 0.0f) return 0.5f;
        if (y == 0.0f) return 0.0f;
        return -0.5f;
    }

    float atan;
    const float z = y / x;

    if (fabsf(z) < 1.0f)
    {
        atan = z / (1.0f*(float)M_PI + 0.28086f*(float)M_PI*z*z);
        if (x < 0.0f)
        {
            if (y < 0.0f) return atan - 1.0f;
            return atan + 1.0f;
        }
    }
    else
    {
        atan = 0.5f - z / (z*z + 0.28086f) * (float)M_1_PI;
        if ( y < 0.0f ) return atan - 1.0f;
    }

    return atan;
}


#endif /* ATAN2_H */
