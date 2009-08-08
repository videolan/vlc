// Macro for killing denormalled numbers
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// Based on IS_DENORMAL macro by Jon Watte
// This code is public domain

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <math.h>
#include "denormals.h"

/* fpclassify() is C99, cannot be compiled into a C++90 file (on some systems) */
float undenormalise( float f )
{
    if( fpclassify( f ) == FP_SUBNORMAL  )
        return 0.0;
    return f;
}
