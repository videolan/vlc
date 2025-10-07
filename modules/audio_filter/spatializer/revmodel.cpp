// Reverb model implementation
//
//
// Google Summer of Code 2007
//
// Authors: Biodun Osunkunle <biodun@videolan.org>
//
// Mentor : Jean-Baptiste Kempf <jb@videolan.org>
//
// Original written by Jezar at Dreampoint, June 2000

// This code is public domain

#include "revmodel.hpp"
#include "tuning.h"
#include <stdlib.h>

revmodel::revmodel() : roomsize(initialroom*scaleroom + offsetroom),
                       damp(initialdamp*scaledamp),
                       wet(initialwet*scalewet),
                       dry(initialdry*scaledry),
                       width(initialwidth),
                       mode(initialmode),
                       // Tie the components to their buffers
                       combL {    {bufcombL1, combtuningL1}, {bufcombL2, combtuningL2},
                                  {bufcombL3, combtuningL3}, {bufcombL4, combtuningL4},
                                  {bufcombL5, combtuningL5}, {bufcombL6, combtuningL6},
                                  {bufcombL7, combtuningL7}, {bufcombL8, combtuningL8} },
                       combR {    {bufcombR1, combtuningR1}, {bufcombR2, combtuningR2},
                                  {bufcombR3, combtuningR3}, {bufcombR4, combtuningR4},
                                  {bufcombR5, combtuningR5}, {bufcombR6, combtuningR6},
                                  {bufcombR7, combtuningR7}, {bufcombR8, combtuningR8} },
                       allpassL { {bufallpassL1, allpasstuningL1}, {bufallpassL2, allpasstuningL2},
                                  {bufallpassL3, allpasstuningL3}, {bufallpassL4, allpasstuningL4} },
                       allpassR { {bufallpassR1, allpasstuningR1}, {bufallpassR2, allpasstuningR2},
                                  {bufallpassR3, allpasstuningR3}, {bufallpassR4, allpasstuningR4} }
{
    update();

    // Buffer will be full of rubbish - so we MUST mute them
    mute();
}

void revmodel::mute()
{
    int i;
    if (mode >= freezemode)
        return;

    for (i = 0 ; i < numcombs ; i++)
    {
        combL[i].mute();
        combR[i].mute();
    }
    for (i=0;i<numallpasses;i++)
    {
        allpassL[i].mute();
        allpassR[i].mute();
    }
}

/*****************************************************************************
 *  Transforms the audio stream
 * /param float *inputL     input buffer
 * /param float *outputL   output buffer
 * /param long numsamples  number of samples to be processed
 * /param int skip             number of channels in the audio stream
 *****************************************************************************/
void revmodel::processreplace(float *inputL, float *outputL, long /* numsamples */, int skip)
{
    float outL,outR,input;
    float inputR;
    int i;

    outL = outR = 0;
        /* TODO this module supports only 2 audio channels, let's improve this */
        if (skip > 1)
           inputR = inputL[1];
        else
           inputR = inputL[0];
        input = (inputL[0] + inputR) * gain;

        // Accumulate comb filters in parallel
        for(i=0; i<numcombs; i++)
        {
            outL += combL[i].process(input);
            outR += combR[i].process(input);
        }

        // Feed through allpasses in series
        for(i=0; i<numallpasses; i++)
        {
            outL = allpassL[i].process(outL);
            outR = allpassR[i].process(outR);
        }

        // Calculate output REPLACING anything already there
        outputL[0] = (outL*wet1 + outR*wet2 + inputR*dry);
           if (skip > 1)
        outputL[1] = (outR*wet1 + outL*wet2 + inputR*dry);
}

void revmodel::processmix(float *inputL, float *outputL, long /* numsamples */, int skip)
{
    float outL,outR,input;
    float inputR;

    outL = outR = 0;
        if (skip > 1)
           inputR = inputL[1];
        else
           inputR = inputL[0];
        input = (inputL[0] + inputR) * gain;

        // Accumulate comb filters in parallel
        for(int i=0; i<numcombs; i++)
        {
            outL += combL[i].process(input);
            outR += combR[i].process(input);
        }

        // Feed through allpasses in series
        for(int i=0; i<numallpasses; i++)
        {
            outL = allpassL[i].process(outL);
            outR = allpassR[i].process(outR);
        }

        // Calculate output REPLACING anything already there
        outputL[0] += (outL*wet1 + outR*wet2 + inputR*dry);
           if (skip > 1)
        outputL[1] += (outR*wet1 + outL*wet2 + inputR*dry);
}

void revmodel::update()
{
// Recalculate internal values after parameter change

    wet1 = wet*(width/2 + 0.5f);
    wet2 = wet*((1-width)/2);

    if (mode >= freezemode)
    {
        roomsize1 = 1;
        damp1 = 0;
        gain = muted;
    }
    else
    {
        roomsize1 = roomsize;
        damp1 = damp;
        gain = fixedgain;
    }

    for(int i=0; i<numcombs; i++)
    {
        combL[i].setfeedback(roomsize1);
        combR[i].setfeedback(roomsize1);
    }

    for(int i=0; i<numcombs; i++)
    {
        combL[i].setdamp(damp1);
        combR[i].setdamp(damp1);
    }
}

// The following set functions are not inlined, because
// speed is never an issue when calling them, and also
// because as you develop the reverb model, you may
// wish to take dynamic action when they are called.

void revmodel::setroomsize(float value)
{
    roomsize = (value*scaleroom) + offsetroom;
    update();
}

void revmodel::setdamp(float value)
{
    damp = value*scaledamp;
    update();
}

void revmodel::setwet(float value)
{
    wet = value*scalewet;
    update();
}

void revmodel::setdry(float value)
{
    dry = value*scaledry;
}

void revmodel::setwidth(float value)
{
    width = value;
    update();
}

void revmodel::setmode(float value)
{
    mode = value;
    update();
}

//ends
