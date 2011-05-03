/*
 * AtmoOutputFilter.cpp: Post Processor for the color data retrieved from
 * a CAtmoInput
 *
 * mostly 1:1 from vdr-linux-src "filter.c" copied
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include "AtmoOutputFilter.h"



CAtmoOutputFilter::CAtmoOutputFilter(CAtmoConfig *atmoConfig )
{
   this->m_pAtmoConfig = atmoConfig;
   this->m_percent_filter_output_old = NULL;
   this->m_mean_filter_output_old = NULL;
   this->m_mean_values = NULL;
   this->m_mean_sums = NULL;
   ResetFilter();
}

CAtmoOutputFilter::~CAtmoOutputFilter(void)
{
  if(m_percent_filter_output_old)
     delete (char *)m_percent_filter_output_old;

  if(m_mean_filter_output_old)
     delete (char *)m_mean_filter_output_old;

  if(m_mean_values)
     delete (char *)m_mean_values;

  if(m_mean_sums)
     delete (char *)m_mean_sums;
}

void CAtmoOutputFilter::ResetFilter(void)
{
  // reset filter values
  MeanFilter(NULL, true);
  PercentFilter(NULL, true);
}

pColorPacket CAtmoOutputFilter::Filtering(pColorPacket ColorPacket)
{
  switch (m_pAtmoConfig->getLiveViewFilterMode())
  {
    case afmNoFilter:
         return ColorPacket;
    break;

    case afmCombined:
         return MeanFilter(ColorPacket, false);
    break;

    case afmPercent:
         return PercentFilter(ColorPacket, false);
    break;
  }

  return ColorPacket;
}

pColorPacket CAtmoOutputFilter::PercentFilter(pColorPacket filter_input, ATMO_BOOL init)
{
  // last values needed for the percentage filter
  if (init) // Initialization
  {
    if(m_percent_filter_output_old)
       delete (char *)m_percent_filter_output_old;
    m_percent_filter_output_old = NULL;
    return(NULL);
  }

  if(!m_percent_filter_output_old || (m_percent_filter_output_old->numColors!=filter_input->numColors)) {
     delete m_percent_filter_output_old;
     AllocColorPacket(m_percent_filter_output_old, filter_input->numColors);
     ZeroColorPacket(m_percent_filter_output_old);
  }

  int percentNew = this->m_pAtmoConfig->getLiveViewFilter_PercentNew();

  pColorPacket filter_output;
  AllocColorPacket(filter_output, filter_input->numColors);

  for (int zone = 0; zone < filter_input->numColors; zone++)
  {
	filter_output->zone[zone].r = (filter_input->zone[zone].r *
         (100-percentNew) + m_percent_filter_output_old->zone[zone].r * percentNew) / 100;
	
    filter_output->zone[zone].g = (filter_input->zone[zone].g *
         (100-percentNew) + m_percent_filter_output_old->zone[zone].g * percentNew) / 100;

	filter_output->zone[zone].b = (filter_input->zone[zone].b *
         (100-percentNew) + m_percent_filter_output_old->zone[zone].b * percentNew) / 100;
  }

  CopyColorPacket( filter_output, m_percent_filter_output_old );

  delete (char *)filter_input;

  return filter_output;
}

pColorPacket CAtmoOutputFilter::MeanFilter(pColorPacket filter_input, ATMO_BOOL init)
{
  // needed vor the running mean value filter

  // needed for the percentage filter
  static int filter_length_old;
  char reinitialize = 0;
  long int tmp;
  pColorPacket filter_output;

  if (init) // Initialization
  {
    if(m_mean_filter_output_old)
       delete (char *)m_mean_filter_output_old;
    m_mean_filter_output_old = NULL;

    if(m_mean_values)
       delete (char *)m_mean_values;
    m_mean_values = NULL;

    if(m_mean_sums)
       delete (char *)m_mean_sums;
    m_mean_sums = NULL;
    return (NULL);
  }

  if(!m_mean_filter_output_old || (m_mean_filter_output_old->numColors!=filter_input->numColors)) {
        delete m_mean_filter_output_old;
        AllocColorPacket(m_mean_filter_output_old, filter_input->numColors);
        ZeroColorPacket(m_mean_filter_output_old);
  }

  if(!m_mean_values || (m_mean_values->numColors!=filter_input->numColors)) {
        delete m_mean_values;
        AllocColorPacket(m_mean_values, filter_input->numColors);
        ZeroColorPacket(m_mean_values);
  }

  if(!m_mean_sums || (m_mean_sums->numColors!=filter_input->numColors)) {
        delete m_mean_sums;
        AllocLongColorPacket(m_mean_sums, filter_input->numColors);
        ZeroLongColorPacket(m_mean_sums);
  }

  AllocColorPacket(filter_output, filter_input->numColors);


  int AtmoSetup_Filter_MeanLength = m_pAtmoConfig->getLiveViewFilter_MeanLength();
  int AtmoSetup_Filter_PercentNew = m_pAtmoConfig->getLiveViewFilter_PercentNew();
  int AtmoSetup_Filter_MeanThreshold = m_pAtmoConfig->getLiveViewFilter_MeanThreshold();

  // if filter_length has changed
  if (filter_length_old != AtmoSetup_Filter_MeanLength)
  {
    // force reinitialization of the filter
    reinitialize = 1;
  }
  filter_length_old = AtmoSetup_Filter_MeanLength;

  if (filter_length_old < 20) filter_length_old = 20; // avoid division by 0

  for (int zone = 0; zone < filter_input->numColors; zone++)
  {
    // calculate the mean-value filters
      m_mean_sums->longZone[zone].r +=
        (long int)(filter_input->zone[zone].r - m_mean_values->zone[zone].r); // red
    tmp = m_mean_sums->longZone[zone].r / ((long int)filter_length_old / 20);
    if(tmp<0) tmp = 0; else { if(tmp>255) tmp = 255; }
    m_mean_values->zone[zone].r = (unsigned char)tmp;

    m_mean_sums->longZone[zone].g +=
        (long int)(filter_input->zone[zone].g - m_mean_values->zone[zone].g); // green
    tmp = m_mean_sums->longZone[zone].g / ((long int)filter_length_old / 20);
    if(tmp<0) tmp = 0; else { if(tmp>255) tmp = 255; }
    m_mean_values->zone[zone].g = (unsigned char)tmp;

    m_mean_sums->longZone[zone].b +=
        (long int)(filter_input->zone[zone].b - m_mean_values->zone[zone].b); // blue
    tmp = m_mean_sums->longZone[zone].b / ((long int)filter_length_old / 20);
    if(tmp<0) tmp = 0; else { if(tmp>255) tmp = 255; }
    m_mean_values->zone[zone].b = (unsigned char)tmp;

    // check, if there is a jump -> check if differences between actual values and filter values are too big

    long int dist; // distance between the two colors in the 3D RGB space
    dist = (m_mean_values->zone[zone].r - filter_input->zone[zone].r) *
           (m_mean_values->zone[zone].r - filter_input->zone[zone].r) +
           (m_mean_values->zone[zone].g - filter_input->zone[zone].g) *
           (m_mean_values->zone[zone].g - filter_input->zone[zone].g) +
           (m_mean_values->zone[zone].b - filter_input->zone[zone].b) *
           (m_mean_values->zone[zone].b - filter_input->zone[zone].b);

    /*
       if (dist > 0) { dist = (long int)sqrt((double)dist); }
       avoid sqrt(0) (TODO: necessary?)
       I think its cheaper to calculate the square of something ..? insteas geting the square root?
    */
    double distMean = ((double)AtmoSetup_Filter_MeanThreshold * 3.6f);
    distMean = distMean * distMean;

    /*
      compare calculated distance with the filter threshold
  	  if ((dist > (long int)((double)AtmoSetup.Filter_MeanThreshold * 3.6f)) || ( reinitialize == 1))
   */

	if ((dist > distMean) || ( reinitialize == 1))
    {
      // filter jump detected -> set the long filters to the result of the short filters
      filter_output->zone[zone] = m_mean_values->zone[zone] = filter_input->zone[zone];

      m_mean_sums->longZone[zone].r = filter_input->zone[zone].r *
                                (filter_length_old / 20);
      m_mean_sums->longZone[zone].g = filter_input->zone[zone].g *
                                (filter_length_old / 20);
      m_mean_sums->longZone[zone].b = filter_input->zone[zone].b *
                                (filter_length_old / 20);
    }
    else
    {
      // apply an additional percent filter and return calculated values

	  filter_output->zone[zone].r = (m_mean_values->zone[zone].r *
          (100-AtmoSetup_Filter_PercentNew) +
          m_mean_filter_output_old->zone[zone].r * AtmoSetup_Filter_PercentNew) / 100;

	  filter_output->zone[zone].g = (m_mean_values->zone[zone].g *
          (100-AtmoSetup_Filter_PercentNew) +
          m_mean_filter_output_old->zone[zone].g * AtmoSetup_Filter_PercentNew) / 100;

	  filter_output->zone[zone].b = (m_mean_values->zone[zone].b *
          (100-AtmoSetup_Filter_PercentNew) +
          m_mean_filter_output_old->zone[zone].b * AtmoSetup_Filter_PercentNew) / 100;
    }
  }

  CopyColorPacket(filter_output, m_mean_filter_output_old);

  delete (char *)filter_input;

  return(filter_output);
}
