// Comb filter class declaration
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#ifndef _comb_
#define _comb_

#include "denormals.h"

/**
* Combination filter
*Takes multiple audio channels and mix them for one ear
*/
class comb
{
public:
    comb();
    void    setbuffer(float *buf, int size);
    inline  float    process(float inp);
    void    mute();
    void    setdamp(float val);
    float    getdamp();
    void    setfeedback(float val);
    float    getfeedback();
private:
    float    feedback;
    float    filterstore;
    float    damp1;
    float    damp2;
    float    *buffer;
    int    bufsize;
    int    bufidx;
};


// Big to inline - but crucial for speed

inline float comb::process(float input)
{
/* FIXME
* comb::process is not really ear-friendly the tunning values must
* be changed*/
    float output;

    output = undenormalise( buffer[bufidx] );

    filterstore = undenormalise(output*damp2);

    buffer[bufidx] = input + filterstore*feedback;

    if(++bufidx>=bufsize) bufidx = 0;

    return output;
}

#endif //_comb_

//ends
