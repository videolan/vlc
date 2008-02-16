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

#include <string.h>
#include "AtmoOutputFilter.h"



CAtmoOutputFilter::CAtmoOutputFilter(CAtmoConfig *atmoConfig)
{
   this->m_pAtmoConfig = atmoConfig;
   ResetFilter();
}

CAtmoOutputFilter::~CAtmoOutputFilter(void)
{
}

void CAtmoOutputFilter::ResetFilter(void)
{
  // reset filter values
  MeanFilter(true);
  PercentFilter(true);
}

tColorPacket CAtmoOutputFilter::Filtering(tColorPacket ColorPacket)
{
  filter_input = ColorPacket;

  switch (m_pAtmoConfig->getLiveViewFilterMode())
  {
    case afmNoFilter:
         filter_output = filter_input;
    break;

    case afmCombined:
         MeanFilter(false);
    break;

    case afmPercent:
         PercentFilter(false);
    break;

    default:
         filter_output = filter_input;
    break;
  }

  return filter_output;
}

void CAtmoOutputFilter::PercentFilter(ATMO_BOOL init)
{
  // last values needed for the percentage filter
  static tColorPacket filter_output_old;

  if (init) // Initialization
  {
    memset(&filter_output_old, 0, sizeof(filter_output_old));
    return;
  }

  int percentNew = this->m_pAtmoConfig->getLiveViewFilter_PercentNew();

  for (int ch = 0; ch < ATMO_NUM_CHANNELS; ch++)
  {
	filter_output.channel[ch].r = (filter_input.channel[ch].r *
         (100-percentNew) + filter_output_old.channel[ch].r * percentNew) / 100;
	
    filter_output.channel[ch].g = (filter_input.channel[ch].g *
         (100-percentNew) + filter_output_old.channel[ch].g * percentNew) / 100;

	filter_output.channel[ch].b = (filter_input.channel[ch].b *
         (100-percentNew) + filter_output_old.channel[ch].b * percentNew) / 100;
  }

  filter_output_old = filter_output;
}

void CAtmoOutputFilter::MeanFilter(ATMO_BOOL init)
{
  // needed vor the running mean value filter
  static tColorPacketLongInt mean_sums;
  static tColorPacket mean_values;
  // needed for the percentage filter
  static tColorPacket filter_output_old;
  static int filter_length_old;
  char reinitialize = 0;
  long int tmp;

  if (init) // Initialization
  {
    memset(&filter_output_old, 0, sizeof(filter_output_old));
    memset(&mean_sums, 0, sizeof(mean_sums));
    memset(&mean_values, 0, sizeof(mean_values));
    return;
  }
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

  for (int ch = 0; ch < ATMO_NUM_CHANNELS; ch++)
  {
    // calculate the mean-value filters
    mean_sums.channel[ch].r +=
         (long int)(filter_input.channel[ch].r - mean_values.channel[ch].r); // red
    tmp = mean_sums.channel[ch].r / ((long int)filter_length_old / 20);
    if(tmp<0) tmp = 0; else { if(tmp>255) tmp = 255; }
    mean_values.channel[ch].r = (unsigned char)tmp;

    mean_sums.channel[ch].g +=
        (long int)(filter_input.channel[ch].g - mean_values.channel[ch].g); // green
    tmp = mean_sums.channel[ch].g / ((long int)filter_length_old / 20);
    if(tmp<0) tmp = 0; else { if(tmp>255) tmp = 255; }
    mean_values.channel[ch].g = (unsigned char)tmp;

    mean_sums.channel[ch].b +=
        (long int)(filter_input.channel[ch].b - mean_values.channel[ch].b); // blue
    tmp = mean_sums.channel[ch].b / ((long int)filter_length_old / 20);
    if(tmp<0) tmp = 0; else { if(tmp>255) tmp = 255; }
    mean_values.channel[ch].b = (unsigned char)tmp;

    // check, if there is a jump -> check if differences between actual values and filter values are too big

    long int dist; // distance between the two colors in the 3D RGB space
    dist = (mean_values.channel[ch].r - filter_input.channel[ch].r) *
           (mean_values.channel[ch].r - filter_input.channel[ch].r) +
           (mean_values.channel[ch].g - filter_input.channel[ch].g) *
           (mean_values.channel[ch].g - filter_input.channel[ch].g) +
           (mean_values.channel[ch].b - filter_input.channel[ch].b) *
           (mean_values.channel[ch].b - filter_input.channel[ch].b);

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
      filter_output.channel[ch] = mean_values.channel[ch] = filter_input.channel[ch];

      mean_sums.channel[ch].r = filter_input.channel[ch].r *
                                (filter_length_old / 20);
      mean_sums.channel[ch].g = filter_input.channel[ch].g *
                                (filter_length_old / 20);
      mean_sums.channel[ch].b = filter_input.channel[ch].b *
                                (filter_length_old / 20);
    }
    else
    {
      // apply an additional percent filter and return calculated values

	  filter_output.channel[ch].r = (mean_values.channel[ch].r *
          (100-AtmoSetup_Filter_PercentNew) +
          filter_output_old.channel[ch].r * AtmoSetup_Filter_PercentNew) / 100;

	  filter_output.channel[ch].g = (mean_values.channel[ch].g *
          (100-AtmoSetup_Filter_PercentNew) +
          filter_output_old.channel[ch].g * AtmoSetup_Filter_PercentNew) / 100;

	  filter_output.channel[ch].b = (mean_values.channel[ch].b *
          (100-AtmoSetup_Filter_PercentNew) +
          filter_output_old.channel[ch].b * AtmoSetup_Filter_PercentNew) / 100;
    }
  }
  filter_output_old = filter_output;
}
