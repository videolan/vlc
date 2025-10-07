// Comb filter implementation
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#include "comb.hpp"
#include <stddef.h>

comb::comb(float *buf, int size) :
    feedback(0.0f),
    filterstore(0.0f),
    damp1(0.0f),
    damp2(0.0f),
    buffer(buf),
    bufsize(size),
    bufidx(0)
{
}

void comb::mute()
{
    for (int i=0; i<bufsize; i++)
        buffer[i]=0;
}

void comb::setdamp(float val)
{
    damp1 = val;
    damp2 = 1-val;
}

void comb::setfeedback(float val)
{
    feedback = val;
}

// ends
