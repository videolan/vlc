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

#define Weight(zone, pixel_nummer) m_Weight[((zone) * (IMAGE_SIZE)) + (pixel_nummer)]

class CAtmoColorCalculator
{
protected:
   CAtmoConfig *m_pAtmoConfig;

  // Flip instead having a array with (64x48) entries of values for each channel
  // I have x arrays of 64x48 so each channel has its own array...
  // (or gradient which is use to judge about the pixels)
  int *m_Weight;
  int **m_Zone_Weights;

  long int *m_hue_hist;
  long int *m_windowed_hue_hist;
  int *m_most_used_hue_last;
  int *m_most_used_hue;

  long int *m_sat_hist;
  long int *m_windowed_sat_hist;
  int *m_most_used_sat;

  long int *m_average_v;
  int *m_average_counter;

protected:
  int m_LastEdgeWeighting;
  int m_LastWidescreenMode;
  int m_LastLayout_TopCount;
  int m_LastLayout_BottomCount;
  int m_LastLayout_LRCount;
  int m_LastNumZones;


protected:
    void FindMostUsed(int AtmoSetup_NumZones,int *most_used,long int *windowed_hist);

public:
    CAtmoColorCalculator(CAtmoConfig *pAtmoConfig);
	~CAtmoColorCalculator(void);

    pColorPacket AnalyzeHSV(tHSVColor *HSV_Img);

    void UpdateParameters();
};


tHSVColor RGB2HSV(tRGBColor color);
tRGBColor HSV2RGB(tHSVColor color);

#endif
