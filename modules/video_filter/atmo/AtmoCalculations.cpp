/*
 * AtmoCalculations.cpp: calculations needed by the input devices
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "AtmoDefs.h"

#if defined(_WIN32)
#   include <windows.h>
#endif

#include "AtmoCalculations.h"
#include "AtmoConfig.h"
#include "AtmoZoneDefinition.h"


// set accuracy of color calculation
#define h_MAX   255
#define s_MAX   255
#define v_MAX   255

// macro
#define POS_DIV(a, b)  ( (a)/(b) + ( ((a)%(b) >= (b)/2 ) ? 1 : 0) )


CAtmoColorCalculator::CAtmoColorCalculator(CAtmoConfig *pAtmoConfig)
{
  m_pAtmoConfig = pAtmoConfig;
  m_Weight                = NULL;
  m_hue_hist              = NULL;
  m_windowed_hue_hist     = NULL;
  m_most_used_hue_last    = NULL;
  m_most_used_hue         = NULL;
  m_sat_hist              = NULL;
  m_windowed_sat_hist     = NULL;
  m_most_used_sat         = NULL;
  m_Zone_Weights          = NULL;
  m_average_v             = NULL;
  m_average_counter       = NULL;

  m_LastEdgeWeighting      = -1;
  m_LastWidescreenMode     = -1;
  m_LastLayout_TopCount    = -1;
  m_LastLayout_BottomCount = -1;
  m_LastLayout_LRCount     = -1;
  m_LastNumZones           = -1;
}

CAtmoColorCalculator::~CAtmoColorCalculator(void)
{
  delete[] m_Weight;
  delete[] m_hue_hist;
  delete[] m_windowed_hue_hist;
  delete[] m_most_used_hue_last;
  delete[] m_most_used_hue;
  delete[] m_sat_hist;
  delete[] m_windowed_sat_hist;
  delete[] m_most_used_sat;
  delete[] m_Zone_Weights;
  delete[] m_average_v;
  delete[] m_average_counter;
}

void CAtmoColorCalculator::UpdateParameters()
{
  // Zonen Definition neu laden
  // diverse Vorberechnungen neu ausführen
  // Speicherbuffer neu allokieren!
}

void CAtmoColorCalculator::FindMostUsed(int AtmoSetup_NumZones,int *most_used,long int *windowed_hist)
{
  memset(most_used, 0, sizeof(int) * AtmoSetup_NumZones);


  for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
  {
    int value = 0;
    // walk trough histogram
    for (int i = 0; i < s_MAX+1; i++) // assume s_MAX = h_MAX = v_Max
    {
      // if new value bigger then old one
      int tmp = *windowed_hist;  // windowed_hist[zone * (s_MAX+1) + i];
      // if (w_sat_hist[channel][i] > value)
      if (tmp > value)
      {
        // remember index
        most_used[zone] = i;
        // and value
        value = tmp;
      }
      windowed_hist++;
    }
  }
}

pColorPacket CAtmoColorCalculator::AnalyzeHSV(tHSVColor *HSV_Img)
{
  int i; // counter

  int AtmoSetup_EdgeWeighting  = m_pAtmoConfig->getLiveView_EdgeWeighting();
  int AtmoSetup_WidescreenMode = m_pAtmoConfig->getLiveView_WidescreenMode();
  int AtmoSetup_DarknessLimit  = m_pAtmoConfig->getLiveView_DarknessLimit();
  int AtmoSetup_BrightCorrect  = m_pAtmoConfig->getLiveView_BrightCorrect();
  int AtmoSetup_SatWinSize     = m_pAtmoConfig->getLiveView_SatWinSize();
  int AtmoSetup_NumZones       = m_pAtmoConfig->getZoneCount();
  tHSVColor *temp_Img;

  if(AtmoSetup_NumZones != m_LastNumZones)
  {
     delete[] m_Weight;
     delete[] m_hue_hist;
     delete[] m_windowed_hue_hist;
     delete[] m_most_used_hue_last;
     delete[] m_most_used_hue;
     delete[] m_sat_hist;
     delete[] m_windowed_sat_hist;
     delete[] m_most_used_sat;
     delete[] m_Zone_Weights;
     delete[] m_average_v;
     delete[] m_average_counter;

     m_Weight              = new int[AtmoSetup_NumZones * IMAGE_SIZE];
     m_Zone_Weights        = new int*[AtmoSetup_NumZones];
     for(int i = 0; i < AtmoSetup_NumZones; i++)
         m_Zone_Weights[i] = &m_Weight[i * IMAGE_SIZE];

     m_hue_hist            = new long int[(h_MAX+1) * AtmoSetup_NumZones];
     m_windowed_hue_hist   = new long int[(h_MAX+1) * AtmoSetup_NumZones];

     m_most_used_hue_last  = new int[AtmoSetup_NumZones];
     m_most_used_hue       = new int[AtmoSetup_NumZones];
     memset( m_most_used_hue_last, 0, sizeof(int) * AtmoSetup_NumZones);

     m_sat_hist           = new long int[(s_MAX+1) * AtmoSetup_NumZones];
     m_windowed_sat_hist  = new long int[(s_MAX+1) * AtmoSetup_NumZones];
     m_most_used_sat      = new int[AtmoSetup_NumZones];

     m_average_v         = new long int[AtmoSetup_NumZones];
     m_average_counter   = new int[AtmoSetup_NumZones];

     m_LastNumZones = AtmoSetup_NumZones;
  }


  // calculate only if setup has changed
  if ((AtmoSetup_EdgeWeighting != m_LastEdgeWeighting) ||
      (AtmoSetup_WidescreenMode != m_LastWidescreenMode) ||
      (m_pAtmoConfig->getZonesTopCount() != m_LastLayout_TopCount) ||
      (m_pAtmoConfig->getZonesBottomCount() != m_LastLayout_BottomCount) ||
      (m_pAtmoConfig->getZonesLRCount() !=  m_LastLayout_LRCount) ||
      (m_pAtmoConfig->m_UpdateEdgeWeightningFlag != 0)
     )
  {

      for(i = 0 ;i < AtmoSetup_NumZones; i++) {
          CAtmoZoneDefinition *pZoneDef = m_pAtmoConfig->getZoneDefinition(i);
          if(pZoneDef)
          {
             pZoneDef->UpdateWeighting(m_Zone_Weights[i],
                                                              AtmoSetup_WidescreenMode,
                                                              AtmoSetup_EdgeWeighting);
#ifdef _debug_zone_weight_
             char filename[128];
             sprintf(filename, "zone_%d_gradient_debug.bmp",i);
             pZoneDef->SaveZoneBitmap( filename );
             sprintf(filename, "zone_%d_weight_%d_debug.bmp",i,AtmoSetup_EdgeWeighting);
             pZoneDef->SaveWeightBitmap(filename, m_Zone_Weights[i] );
#endif
          }

     }
     m_pAtmoConfig->m_UpdateEdgeWeightningFlag = 0;

     m_LastEdgeWeighting      = AtmoSetup_EdgeWeighting;
     m_LastWidescreenMode     = AtmoSetup_WidescreenMode;
     m_LastLayout_TopCount    = m_pAtmoConfig->getZonesTopCount();
     m_LastLayout_BottomCount = m_pAtmoConfig->getZonesBottomCount();
     m_LastLayout_LRCount     = m_pAtmoConfig->getZonesLRCount();
  }

  AtmoSetup_DarknessLimit = AtmoSetup_DarknessLimit * 10;


  /***************************************************************************/
  /* Hue                                                                     */
  /***************************************************************************/
  /*----------------------------*/
  /* hue histogram builtup      */
  /*----------------------------*/
  // HSV histogram
  // long int hue_hist[CAP_MAX_NUM_ZONES][h_MAX+1];

  // average brightness (value)
  // m_average_v m_average_counter

  // clean histogram --> calloc
  memset(m_hue_hist, 0, sizeof(long int) * (h_MAX+1) * AtmoSetup_NumZones);
  memset(m_average_v, 0, sizeof(long int) * AtmoSetup_NumZones);
  memset(m_average_counter, 0, sizeof(int) * AtmoSetup_NumZones);

  temp_Img = HSV_Img;
  i = 0;
  for (int row = 0; row < CAP_HEIGHT; row++)
  {
    for (int column = 0; column < CAP_WIDTH; column++)
    {
      // forget black bars: perform calculations only if pixel has some luminosity
	  if ((*temp_Img).v > AtmoSetup_DarknessLimit)
      {
        // builtup histogram for the x Zones of the Display
        for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
        {
          // Add weight to channel
         // Weight(zone, pixel_nummer) m_Weight[((zone) * (IMAGE_SIZE)) + (pixel_nummer)]
         // m_hue_hist[zone*(h_MAX+1) + HSV_Img[i].h] += m_Zone_Weights[zone][i] * HSV_Img[i].v;
            m_hue_hist[zone*(h_MAX+1) + (*temp_Img).h] += m_Zone_Weights[zone][i] * temp_Img->v;

            if(m_Zone_Weights[zone][i] > 0) {
               m_average_v[zone] += temp_Img->v;
               m_average_counter[zone]++;
            }

        }
        // calculate brightness average
      }
      temp_Img++;
      i++;
    }
  }

  /*----------------------------*/
  /* hue histogram windowing    */
  /*----------------------------*/
  // windowed HSV histogram
  // long int w_hue_hist[CAP_MAX_NUM_ZONES][h_MAX+1]; -> m_windowed_hue_hist
  // clean windowed histogram
  memset(m_windowed_hue_hist, 0, sizeof(long int) * (h_MAX+1) * AtmoSetup_NumZones);
  // steps in each direction; eg. 2 => -2 -1 0 1 2 windowing
  int hue_windowsize = m_pAtmoConfig->getLiveView_HueWinSize();

  for (i = 0; i < h_MAX+1; i++) // walk through histogram [0;h_MAX]
  {
    // windowing from -hue_windowsize -> +hue_windowsize
    for (int mywin = -hue_windowsize; mywin < hue_windowsize+1; mywin++)
    {
      // addressed histogram candlestick
      int myidx = i + mywin;

      // handle beginning of windowing -> roll back
      if (myidx < 0)     { myidx = myidx + h_MAX + 1; }

      // handle end of windowing -> roll forward
      if (myidx > h_MAX) { myidx = myidx - h_MAX - 1; }

      // Apply windowing to all x zones
      for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
      {
        // apply lite triangular window design with gradient of 10% per discrete step
        m_windowed_hue_hist[(zone * (h_MAX+1)) + i] += m_hue_hist[(zone * (h_MAX+1)) + myidx] * ((hue_windowsize+1)-abs(mywin)); // apply window
      }
    }
  }

  /*--------------------------------------*/
  /* analyze histogram for most used hue  */
  /*--------------------------------------*/
  // index of last maximum
  // static int most_used_hue_last[CAP_MAX_NUM_ZONES] = {0, 0, 0, 0, 0}; --> m_most_used_hue_last

  // resulting hue for each channel
  //int most_used_hue[CAP_MAX_NUM_ZONES]; --> m_most_used_hue

  FindMostUsed(AtmoSetup_NumZones, m_most_used_hue, m_windowed_hue_hist);
  for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
  {
    float percent = (float)m_windowed_hue_hist[zone * (h_MAX+1) + m_most_used_hue_last[zone]] / (float)m_windowed_hue_hist[zone * (h_MAX+1) + m_most_used_hue[zone]];
    if (percent > 0.93f) // less than 7% difference?
        m_most_used_hue[zone] = m_most_used_hue_last[zone]; // use last index
     else
        m_most_used_hue_last[zone] = m_most_used_hue[zone];
  }

  /*
  memset(m_most_used_hue, 0, sizeof(int) * AtmoSetup_NumZones);

  for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
  {
    long int value = 0;
    for (i = 0; i < h_MAX+1; i++) // walk through histogram
    {
      long int tmp = m_windowed_hue_hist[ (zone * (h_MAX+1)) + i ];
      if (tmp > value) // if new value bigger then old one
      {
        m_most_used_hue[zone] = i;     // remember index
        value = tmp; // w_hue_hist[zone][i]; // and value
      }
    }

    float percent = (float)m_windowed_hue_hist[zone * (h_MAX+1) + m_most_used_hue_last[zone]] / (float)value;
    if (percent > 0.93f) // less than 7% difference?
    {
      m_most_used_hue[zone] = m_most_used_hue_last[zone]; // use last index
    }

    m_most_used_hue_last[zone] = m_most_used_hue[zone]; // save current index of most used hue
  }
  */

  /***************************************************************************/
  /* saturation                                                              */
  /***************************************************************************/
  // sat histogram
  // long int sat_hist[CAP_MAX_NUM_ZONES][s_MAX+1];  -> m_sat_hist
  // hue of the pixel we are working at
  int pixel_hue = 0;
  // clean histogram
  memset(m_sat_hist, 0, sizeof(long int) * (s_MAX+1) * AtmoSetup_NumZones);

  /*--------------------------------------*/
  /* saturation histogram builtup         */
  /*--------------------------------------*/
  i = 0;
  temp_Img = HSV_Img;
  for (int row = 0; row < CAP_HEIGHT; row++)
  {
    for (int column = 0; column < CAP_WIDTH; column++)
    {
      // forget black bars: perform calculations only if pixel has some luminosity
	  if ((*temp_Img).v > AtmoSetup_DarknessLimit)
      {
        // find histogram position for pixel
        pixel_hue = (*temp_Img).h;

        // TODO:   brightness calculation(if we require it some time)

        for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
        {
          // only use pixel for histogram if hue is near most_used_hue
          if ((pixel_hue > m_most_used_hue[zone] - hue_windowsize) &&
              (pixel_hue < m_most_used_hue[zone] + hue_windowsize))
          {
            // build histogram
            // sat_hist[channel][HSV_Img[i].s] += Weight[i].channel[channel] * HSV_Img[i].v;
            m_sat_hist[zone * (s_MAX+1) + (*temp_Img).s ] += m_Zone_Weights[zone][i] * (*temp_Img).v;

          }
        }
      }
      i++;
      temp_Img++;
    }
  }

  /*--------------------------------------*/
  /* saturation histogram windowing       */
  /*--------------------------------------*/
   // windowed HSV histogram
   // long int w_sat_hist[CAP_MAX_NUM_ZONES][s_MAX+1]; --> m_windowed_sat_hist
   // clean windowed histogram
   memset(m_windowed_sat_hist, 0, sizeof(long int) * (s_MAX+1) * AtmoSetup_NumZones);
   // steps in each direction; eg. 2 => -2 -1 0 1 2 windowing
   int sat_windowsize = AtmoSetup_SatWinSize;

   // walk through histogram [0;h_MAX]
   for (i = 0; i < s_MAX + 1; i++)
   {
     // windowing from -hue_windowsize -> +hue_windowsize
     for (int mywin = -sat_windowsize; mywin < sat_windowsize+1; mywin++)
     {
       // addressed histogram candlestick
       int myidx = i + mywin;

       // handle beginning of windowing -> roll back
       if (myidx < 0)     { myidx = myidx + s_MAX + 1; }

       // handle end of windowing -> roll forward
       if (myidx > h_MAX) { myidx = myidx - s_MAX - 1; }

       for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
       {
         /*
            apply lite triangular window design with
            gradient of 10% per discrete step
         */
         /*
         w_sat_hist[channel][i] += sat_hist[channel][myidx] *
                                  ((sat_windowsize+1)-abs(mywin)); // apply window
         */
         m_windowed_sat_hist[zone * (s_MAX+1) + i] += m_sat_hist[zone* (h_MAX+1) + myidx] *
                                  ((sat_windowsize+1)-abs(mywin)); // apply window
       }
     }
   }

  /*--------------------------------------*/
  /* analyze histogram for most used sat  */
  /*--------------------------------------*/
   // resulting sat (most_used_hue) for each channel
  // int most_used_sat[CAP_MAX_NUM_ZONES];->m_most_used_sat

  FindMostUsed(AtmoSetup_NumZones, m_most_used_sat, m_windowed_sat_hist);
  /*
  memset(m_most_used_sat, 0, sizeof(int) * AtmoSetup_NumZones);

  for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
  {
    int value = 0;
    // walk trough histogram
    for (i = 0; i < s_MAX+1; i++)
    {
      // if new value bigger then old one
      int tmp = m_windowed_sat_hist[zone * (s_MAX+1) + i];
      // if (w_sat_hist[channel][i] > value)
      if (tmp > value)
      {
        // remember index
        m_most_used_sat[zone] = i;
        // and value
        value = tmp;
      }
    }
  }
  */


  /*----------------------------------------------------------*/
  /* calculate average brightness within HSV image            */
  /* uniform Brightness for all channels is calculated        */
  /*----------------------------------------------------------*/
  /* code integrated into "hue histogram builtup" to save some looping time!
  int l_counter = 0;
  // average brightness (value)
  long int value_avg = 0;
  i = 0;
  for (int row = 0; row < CAP_HEIGHT; row++)
  {
    for (int column = 0; column < CAP_WIDTH; column++)
    {
      // find average value: only use bright pixels for luminance average
	  if (HSV_Img[i].v > AtmoSetup_DarknessLimit)
      {
        // build brightness average
        value_avg += HSV_Img[i].v;
        l_counter++;
      }
      i++;
    }
  }
  // calculate brightness average
  if (l_counter > 0) { value_avg = value_avg / l_counter; }
    else { value_avg = AtmoSetup_DarknessLimit; }

  */


  /*----------------------------*/
  /* adjust and copy results    */
  /*----------------------------*/
  tHSVColor hsv_pixel;
  // storage container for resulting RGB values
  pColorPacket output_colors;
  AllocColorPacket(output_colors, AtmoSetup_NumZones);

  // adjust brightness
//  int new_value = (int) ((float)value_avg * ((float)AtmoSetup_BrightCorrect / 100.0));
//  if (new_value > 255) new_value = 255;  // ensure brightness isn't set too high
//  hsv_pixel.v = (unsigned char)new_value;

  /*
  // calculate brightness average
  for(int zone = 0; zone < AtmoSetup_NumZones; zone++) {
      if(m_average_counter[zone] > 0)
          m_average_v[zone] = m_average_v[zone] / m_average_counter[zone]
      else
          m_average_v[zone] = AtmoSetup_DarknessLimit;
  }

  */

  for (int zone = 0; zone < AtmoSetup_NumZones; zone++)
  {
    if(m_average_counter[zone] > 0)
       m_average_v[zone] = m_average_v[zone] / m_average_counter[zone];
    else
       m_average_v[zone] = AtmoSetup_DarknessLimit;

    m_average_v[zone] = (int)((float)m_average_v[zone] * ((float)AtmoSetup_BrightCorrect / 100.0));

    hsv_pixel.v = (unsigned char)ATMO_MAX(ATMO_MIN(m_average_v[zone],255),0);
    hsv_pixel.h = m_most_used_hue[zone];
    hsv_pixel.s = m_most_used_sat[zone];

    // convert back to rgb
    output_colors->zone[zone] = HSV2RGB(hsv_pixel);
  }


  return output_colors;
}

tHSVColor RGB2HSV(tRGBColor color)
{
 int min, max, delta;
 int r, g, b;
 int h = 0;
 tHSVColor hsv;

 r = color.r;
 g = color.g;
 b = color.b;

 min = ATMO_MIN(ATMO_MIN(r, g), b);
 max = ATMO_MAX(ATMO_MAX(r, g), b);

 delta = max - min;

 hsv.v = (unsigned char) POS_DIV( max*v_MAX, 255 );

 if (delta == 0) // This is a gray, no chroma...
 {
   h = 0;        // HSV results = 0 / 1
   hsv.s = 0;
 }
 else // Chromatic data...
 {
   hsv.s = (unsigned char) POS_DIV( (delta*s_MAX) , max );

   int dr = (max - r) + 3*delta;
   int dg = (max - g) + 3*delta;
   int db = (max - b) + 3*delta;
   int divisor = 6*delta;

   if (r == max)
   {
     h = POS_DIV(( (db - dg) * h_MAX ) , divisor);
   }
   else if (g == max)
   {
     h = POS_DIV( ((dr - db) * h_MAX) , divisor) + (h_MAX/3);
   }
   else if (b == max)
   {
     h = POS_DIV(( (dg - dr) * h_MAX) , divisor) + (h_MAX/3)*2;
   }

   if ( h < 0 )     { h += h_MAX; }
   if ( h > h_MAX ) { h -= h_MAX; }
 }
 hsv.h = (unsigned char)h;

 return hsv;
}

tRGBColor HSV2RGB(tHSVColor color)
{
 tRGBColor rgb = {0, 0, 0};

 float h = (float)color.h/(float)h_MAX;
 float s = (float)color.s/(float)s_MAX;
 float v = (float)color.v/(float)v_MAX;

 if (s == 0)
 {
   rgb.r = (int)((v*255.0)+0.5);
   rgb.g = rgb.r;
   rgb.b = rgb.r;
 }
 else
 {
   h = h * 6.0f;
   if (h == 6.0) { h = 0.0; }
   int i = (int)h;

   float f = h - i;
   float p = v*(1.0f-s);
   float q = v*(1.0f-(s*f));
   float t = v*(1.0f-(s*(1.0f-f)));

   if (i == 0)
   {
     rgb.r = (int)((v*255.0)+0.5);
     rgb.g = (int)((t*255.0)+0.5);
     rgb.b = (int)((p*255.0)+0.5);
   }
   else if (i == 1)
   {
     rgb.r = (int)((q*255.0)+0.5);
     rgb.g = (int)((v*255.0)+0.5);
     rgb.b = (int)((p*255.0)+0.5);
   }
   else if (i == 2)
   {
     rgb.r = (int)((p*255.0)+0.5);
     rgb.g = (int)((v*255.0)+0.5);
     rgb.b = (int)((t*255.0)+0.5);
   }
   else if (i == 3)
   {
     rgb.r = (int)((p*255.0)+0.5);
     rgb.g = (int)((q*255.0)+0.5);
     rgb.b = (int)((v*255.0)+0.5);
   }
   else if (i == 4)
   {
     rgb.r = (int)((t*255.0)+0.5);
     rgb.g = (int)((p*255.0)+0.5);
     rgb.b = (int)((v*255.0)+0.5);
   }
   else
   {
     rgb.r = (int)((v*255.0)+0.5);
     rgb.g = (int)((p*255.0)+0.5);
     rgb.b = (int)((q*255.0)+0.5);
   }
 }
 return rgb;
}
