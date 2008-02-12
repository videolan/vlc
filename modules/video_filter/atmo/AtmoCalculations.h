/*
 * AtmoCalculations.h: see calculations.h of the linux version... one to one copy
 * calculations.h: calculations needed by the input devices
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifndef _AtmoCalculations_h_
#define _AtmoCalculations_h_

#include "AtmoDefs.h"
#include "AtmoConfig.h"


tColorPacket CalcColorsAnalyzeHSV(CAtmoConfig *pAtmoConfig, tHSVColor *HSV_Img);

tHSVColor RGB2HSV(tRGBColor color);
tRGBColor HSV2RGB(tHSVColor color);

#endif
