/*
 * AtmoOutputFilter.h: Post Processor for the color data retrieved from a CAtmoInput
 *
 * mostly 1:1 from Linux-src "filter.c" copied
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoOutputFilter_h_
#define _AtmoOutputFilter_h_


#include "AtmoConfig.h"
#include "AtmoDefs.h"

class CAtmoOutputFilter
{
private:
  tColorPacket filter_input;  // input of the filter
  tColorPacket filter_output; // output of the filter

  void PercentFilter(ATMO_BOOL init);
  void MeanFilter(ATMO_BOOL init);

  CAtmoConfig *m_pAtmoConfig;
public:

public:
    CAtmoOutputFilter(CAtmoConfig *atmoConfig);
    virtual ~CAtmoOutputFilter(void);
    void ResetFilter(void);
    tColorPacket Filtering(tColorPacket ColorPacket);
};

#endif
