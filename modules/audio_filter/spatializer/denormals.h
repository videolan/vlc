// Macro for killing denormalled numbers
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// Based on IS_DENORMAL macro by Jon Watte
// This code is public domain

#ifndef _denormals_
#define _denormals_

#include <stdint.h>

static inline float undenormalise( float f )
{
    union { float f; uint32_t u; } data;
    data.f = f;
    if( (data.u & 0x7f800000) == 0 )
        return 0.0;
    return f;
}

#endif//_denormals_

