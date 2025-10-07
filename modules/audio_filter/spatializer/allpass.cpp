// Allpass filter implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#include "allpass.hpp"
#include <stddef.h>

allpass::allpass(float *buf, int size) :
    feedback(0.5f),
    buffer(buf),
    bufsize(size),
    bufidx(0)
{
}

void allpass::mute()
{
    for (int i=0; i<bufsize; i++)
        buffer[i]=0;
}

void allpass::setfeedback(float val)
{
    feedback = val;
}

//ends
