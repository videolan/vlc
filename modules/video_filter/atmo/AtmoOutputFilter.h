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
  //tColorPacket filter_input;  // input of the filter wozu?
  //tColorPacket filter_output; // output of the filter
  pColorPacket  m_percent_filter_output_old;

  pColorPacket  m_mean_filter_output_old;
  pColorPacket  m_mean_values;
  pColorPacketLongInt m_mean_sums;

  pColorPacket PercentFilter(pColorPacket filter_input, ATMO_BOOL init);
  pColorPacket MeanFilter(pColorPacket filter_input, ATMO_BOOL init);

  CAtmoConfig *m_pAtmoConfig;
public:

public:
    CAtmoOutputFilter(CAtmoConfig *atmoConfig);
    virtual ~CAtmoOutputFilter(void);
    void ResetFilter(void);
    pColorPacket Filtering(pColorPacket ColorPacket);
};

#endif
