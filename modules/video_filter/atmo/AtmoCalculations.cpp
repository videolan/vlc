/*
 * calculations.c: calculations needed by the input devices
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id$
 */
#include <stdlib.h>
#include <string.h>


#include "AtmoDefs.h"
#include "AtmoCalculations.h"
#include "AtmoConfig.h"
#include "AtmoZoneDefinition.h"


// set accuracy of color calculation
#define h_MAX   255
#define s_MAX   255
#define v_MAX   255

// macros
#define MIN(X, Y)  ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y)  ((X) > (Y) ? (X) : (Y))
#define POS_DIV(a, b)  ( (a)/(b) + ( ((a)%(b) >= (b)/2 ) ? 1 : 0) )

tColorPacket CalcColorsAnalyzeHSV(CAtmoConfig *pAtmoConfig, tHSVColor *HSV_Img)
{
  int i; // counter

  // static tWeightPacket Weight[IMAGE_SIZE];
  // Flip instead having a array with (64x48) entries of values for each channel
  // I have x arrays of 64x48 so each channel has its own array...
  // (or gradient which is use to judge about the pixels)
  static int Weight[ATMO_NUM_CHANNELS][IMAGE_SIZE];

  /***************************************************************************/
  /* Weight                                                                  */
  /***************************************************************************/
  static int LastEdgeWeighting = -1;
  static int LastWidescreenMode = -1;

  int AtmoSetup_EdgeWeighting  = pAtmoConfig->getLiveView_EdgeWeighting();
  int AtmoSetup_WidescreenMode = pAtmoConfig->getLiveView_WidescreenMode();
  int AtmoSetup_DarknessLimit  = pAtmoConfig->getLiveView_DarknessLimit();
  int AtmoSetup_BrightCorrect  = pAtmoConfig->getLiveView_BrightCorrect();
  int AtmoSetup_SatWinSize     = pAtmoConfig->getLiveView_SatWinSize();


  // calculate only if setup has changed
  if ((AtmoSetup_EdgeWeighting != LastEdgeWeighting) ||
      (AtmoSetup_WidescreenMode != LastWidescreenMode))
  {
     for(i =0 ;i < ATMO_NUM_CHANNELS; i++)
         pAtmoConfig->getZoneDefinition(i)->UpdateWeighting(&Weight[i][0],
                                                            AtmoSetup_WidescreenMode,
                                                            AtmoSetup_EdgeWeighting);
    /*

    original code from VDR sources... my one is just more flexible?*g*

 	i = 0;
    for (int row = 0; row < CAP_HEIGHT; row++)
    {
	  float row_norm = (float)row / ((float)CAP_HEIGHT - 1.0f);       // [0;Height] -> [0;1]
	  float weight_3 = pow(1.0f - row_norm, AtmoSetup_EdgeWeighting); // top
	  float weight_4 = pow(row_norm, AtmoSetup_EdgeWeighting);       // bottom

      for (int column = 0; column < CAP_WIDTH; column++)
      {
        // if widescreen mode, top and bottom of the picture are not
        if ((AtmoSetup_WidescreenMode == 1) && ((row <= CAP_HEIGHT/8) || (row >= (7*CAP_HEIGHT)/8)))
        {
          Weight[i].channel[0] = Weight[i].channel[1] = Weight[i].channel[2] = Weight[i].channel[3] = Weight[i].channel[4] = 0;
        }
        else
        {
          float column_norm = (float)column / ((float)CAP_WIDTH - 1.0f); // [0;Width] -> [0;1]
		  Weight[i].channel[0] = 255;
		  Weight[i].channel[1] = (int)(255.0 * (float)pow((1.0 - column_norm), AtmoSetup_EdgeWeighting));
		  Weight[i].channel[2] = (int)(255.0 * (float)pow(column_norm, AtmoSetup_EdgeWeighting));
          Weight[i].channel[3] = (int)(255.0 * (float)weight_3);
          Weight[i].channel[4] = (int)(255.0 * (float)weight_4);
        }
        i++;
      }
    }
    */
	LastEdgeWeighting = AtmoSetup_EdgeWeighting;
    LastWidescreenMode = AtmoSetup_WidescreenMode;
  }

  /***************************************************************************/
  /* Hue                                                                     */
  /***************************************************************************/

  /*----------------------------*/
  /* hue histogram builtup      */
  /*----------------------------*/
  // HSV histogram
  long int hue_hist[ATMO_NUM_CHANNELS][h_MAX+1];
  // clean histogram
  memset(&hue_hist, 0, sizeof(hue_hist));

  i = 0;
  for (int row = 0; row < CAP_HEIGHT; row++)
  {
    for (int column = 0; column < CAP_WIDTH; column++)
    {
      // forget black bars: perform calculations only if pixel has some luminosity
	  if (HSV_Img[i].v > 10*AtmoSetup_DarknessLimit)
      {
        // builtup histogram for the 5 channels
        for (int channel = 0; channel < ATMO_NUM_CHANNELS; channel++)
        {
          // Add weight to channel
          hue_hist[channel][HSV_Img[i].h] += Weight[channel][i] * HSV_Img[i].v;
        }
      }
      i++;
    }
  }

  /*----------------------------*/
  /* hue histogram windowing    */
  /*----------------------------*/
  // windowed HSV histogram
  long int w_hue_hist[ATMO_NUM_CHANNELS][h_MAX+1];
  // clean windowed histogram
  memset(&w_hue_hist, 0, sizeof(w_hue_hist));
  // steps in each direction; eg. 2 => -2 -1 0 1 2 windowing
  int hue_windowsize = pAtmoConfig->getLiveView_HueWinSize();

  for (i = 0; i < h_MAX+1; i++) // walk through histogram [0;h_MAX]
  {
    // windowing from -hue_windowsize -> +hue_windowsize
    for (int mywin = -hue_windowsize; mywin < hue_windowsize+1; mywin++)
    {
      // adressed histogram candlestick
      int myidx = i + mywin;

      // handle beginning of windowing -> roll back
      if (myidx < 0)     { myidx = myidx + h_MAX + 1; }

      // handle end of windowing -> roll forward
      if (myidx > h_MAX) { myidx = myidx - h_MAX - 1; }

      // Apply windowing to all 5 channels
      for (int channel = 0; channel < ATMO_NUM_CHANNELS; channel++)
      {
        // apply lite triangular window design with gradient of 10% per discrete step
        w_hue_hist[channel][i] += hue_hist[channel][myidx] * ((hue_windowsize+1)-abs(mywin)); // apply window
      }
    }
  }

  /*--------------------------------------*/
  /* analyze histogram for most used hue  */
  /*--------------------------------------*/
  // index of last maximum
  static int most_used_hue_last[ATMO_NUM_CHANNELS] = {0, 0, 0, 0, 0};

  // resulting hue for each channel
  int most_used_hue[ATMO_NUM_CHANNELS];
  memset(&most_used_hue, 0, sizeof(most_used_hue));

  for (int channel = 0; channel < ATMO_NUM_CHANNELS; channel++)
  {
    int value = 0;
    for (i = 0; i < h_MAX+1; i++) // walk through histogram
    {
      if (w_hue_hist[channel][i] > value) // if new value bigger then old one
      {
        most_used_hue[channel] = i;     // remember index
        value = w_hue_hist[channel][i]; // and value
      }
    }

    float percent = (float)w_hue_hist[channel][most_used_hue_last[channel]] / (float)value;
    if (percent > 0.93f) // less than 7% difference?
    {
      most_used_hue[channel] = most_used_hue_last[channel]; // use last index
    }
    most_used_hue_last[channel] = most_used_hue[channel]; // save current index of most used hue
  }

  /***************************************************************************/
  /* saturation                                                              */
  /***************************************************************************/
  // sat histogram
  long int sat_hist[ATMO_NUM_CHANNELS][s_MAX+1];
  // hue of the pixel we are working at
  int pixel_hue = 0;
  // clean histogram
  memset(&sat_hist, 0, sizeof(sat_hist));

  /*--------------------------------------*/
  /* saturation histogram builtup         */
  /*--------------------------------------*/
  i = 0;
  for (int row = 0; row < CAP_HEIGHT; row++)
  {
    for (int column = 0; column < CAP_WIDTH; column++)
    {
      // forget black bars: perform calculations only if pixel has some luminosity
	  if (HSV_Img[i].v > 10*AtmoSetup_DarknessLimit)
      {
        // find histogram position for pixel
        pixel_hue = HSV_Img[i].h;

        // TODO:   brightness calculation(if we require it some time)

        for (int channel = 0; channel < ATMO_NUM_CHANNELS; channel++)
        {
          // only use pixel for histogram if hue is near most_used_hue
          if ((pixel_hue > most_used_hue[channel] - hue_windowsize) &&
              (pixel_hue < most_used_hue[channel] + hue_windowsize))
          {
            // build histogram
            // sat_hist[channel][HSV_Img[i].s] += Weight[i].channel[channel] * HSV_Img[i].v;
            sat_hist[channel][HSV_Img[i].s] += Weight[channel][i] * HSV_Img[i].v;
          }
        }
      }
      i++;
    }
  }

  /*--------------------------------------*/
  /* saturation histogram windowing       */
  /*--------------------------------------*/
   // windowed HSV histogram
   long int w_sat_hist[ATMO_NUM_CHANNELS][s_MAX+1];
   // clean windowed histogram
   memset(&w_sat_hist, 0, sizeof(w_sat_hist));
   // steps in each direction; eg. 2 => -2 -1 0 1 2 windowing
   int sat_windowsize = AtmoSetup_SatWinSize;

   // walk through histogram [0;h_MAX]
   for (i = 0; i < s_MAX + 1; i++)
   {
     // windowing from -hue_windowsize -> +hue_windowsize
     for (int mywin = -sat_windowsize; mywin < sat_windowsize+1; mywin++)
     {
       // adressed histogram candlestick
       int myidx = i + mywin;

       // handle beginning of windowing -> roll back
       if (myidx < 0)     { myidx = myidx + s_MAX + 1; }

       // handle end of windowing -> roll forward
       if (myidx > h_MAX) { myidx = myidx - s_MAX - 1; }

       for (int channel = 0; channel < ATMO_NUM_CHANNELS; channel++)
       {
         /*
            apply lite triangular window design with
            gradient of 10% per discrete step
         */
         w_sat_hist[channel][i] += sat_hist[channel][myidx] *
                                  ((sat_windowsize+1)-abs(mywin)); // apply window
       }
     }
   }

  /*--------------------------------------*/
  /* analyze histogram for most used sat  */
  /*--------------------------------------*/
   // resulting sat (most_used_hue) for each channel
  int most_used_sat[ATMO_NUM_CHANNELS];
  memset(&most_used_sat, 0, sizeof(most_used_sat));

  for (int channel = 0; channel < ATMO_NUM_CHANNELS; channel++)
  {
    int value = 0;
    // walk trough histogram
    for (i = 0; i < s_MAX+1; i++)
    {
      // if new value bigger then old one
      if (w_sat_hist[channel][i] > value)
      {
        // remember index
        most_used_sat[channel] = i;
        // and value
        value = w_sat_hist[channel][i];
      }
    }
  }

  /*----------------------------------------------------------*/
  /* calculate average brightness within HSV image            */
  /* uniform Brightness for all channels is calculated        */
  /*----------------------------------------------------------*/
  int l_counter = 0;
  // average brightness (value)
  long int value_avg = 0;

  // TODO: extract into a function? in sat-histo-built

  i = 0;
  for (int row = 0; row < CAP_HEIGHT; row++)
  {
    for (int column = 0; column < CAP_WIDTH; column++)
    {
      // find average value: only use bright pixels for luminance average
	  if (HSV_Img[i].v > 10*AtmoSetup_DarknessLimit)
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
    else { value_avg = 10 * AtmoSetup_DarknessLimit; }

  /*----------------------------*/
  /* adjust and copy results    */
  /*----------------------------*/
  tHSVColor hsv_pixel;
  // storage container for resulting RGB values
  tColorPacket ColorChannels;

  for (int channel = 0; channel < ATMO_NUM_CHANNELS; channel++)
  {
    // copy values
    hsv_pixel.h = most_used_hue[channel];
    hsv_pixel.s = most_used_sat[channel];

    // adjust brightness
    int new_value = (int) ((float)value_avg * ((float)AtmoSetup_BrightCorrect / 100.0));
    if (new_value > 255) { new_value = 255; } // ensure brightness isn't set too high
    hsv_pixel.v = (unsigned char)new_value;

    // convert back to rgb
    ColorChannels.channel[channel] = HSV2RGB(hsv_pixel);
  }
  return ColorChannels;
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

 min = MIN(MIN(r, g), b);
 max = MAX(MAX(r, g), b);

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
