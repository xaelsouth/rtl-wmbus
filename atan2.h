#ifndef ATAN2_H
#define ATAN2_H

#include <math.h>


static inline float atan2_libm(float complex y)
{
    return cargf(y) * (float)M_1_PI;
}


/** https://gist.github.com/volkansalma/2972237 */
static inline float atan2_approximation(float complex s)
{
    static const float ONEQTR_PI = M_PI / 4.0;
    static const float THRQTR_PI = 3.0 * M_PI / 4.0;

    float y = cimagf(s), x = crealf(s);
    float r, angle;
    float abs_y = fabs(y) + 1e-10f;      // kludge to prevent 0/0 condition

    if (x < 0.0f)
    {
        r = (x + abs_y) / (abs_y - x);
        angle = THRQTR_PI;
    }
    else
    {
        r = (x - abs_y) / (x + abs_y);
        angle = ONEQTR_PI;
    }

    angle += (0.1963f*(float)M_1_PI * r * r - 0.9817f*(float)M_1_PI) * r;

    if (y < 0.0f)
        angle = -angle;     // negate if in quad III or IV

    return angle;
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

    if (fabs(z) < 1.0f)
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
