// Macro for killing denormalled numbers
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// Based on IS_DENORMAL macro by Jon Watte
// This code is public domain

#ifndef _denormals_
#define _denormals_

#include <stdint.h>

#include <math.h>

static inline float undenormalise( float f )
{
    if( fpclassify( f ) == FP_SUBNORMAL  )
        return 0.0;
    return f;
}

#endif//_denormals_

