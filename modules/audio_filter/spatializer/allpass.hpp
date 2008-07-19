// Allpass filter declaration
//
// Written by Jezar at Dreampoint, June 2000
// http://www.dreampoint.co.uk
// This code is public domain

#ifndef _allpass_
#define _allpass_
#include "denormals.h"

class allpass
{
public:
        allpass();
    void    setbuffer(float *buf, int size);
    inline  float    process(float inp);
    void    mute();
    void    setfeedback(float val);
    float    getfeedback();
// private:
    float    feedback;
    float    *buffer;
    int    bufsize;
    int    bufidx;
};


// Big to inline - but crucial for speed

inline float allpass::process(float input)
{
    float output;
    float bufout;

    bufout = undenormalise( buffer[bufidx] );

    output = -input + bufout;
    buffer[bufidx] = input + (bufout*feedback);

    if(++bufidx>=bufsize) bufidx = 0;

    return output;
}

#endif//_allpass

//ends
