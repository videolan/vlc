/*****************************************************************************
 * atmo.cpp : "Atmo Light" video filter
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: André Weber (WeberAndre@gmx.de)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/*****************************************************************************
* Preamble
*****************************************************************************/
#define __STDC_FORMAT_MACROS 1
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <math.h>                                            /* sin(), cos() */
#include <assert.h>

// #define __ATMO_DEBUG__

// [:Zs]+$
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout.h>

#include <vlc_playlist.h>
#include <vlc_filter.h>
#include <vlc_atomic.h>
#include <vlc_charset.h>

#include "filter_picture.h"

#include "AtmoDefs.h"
#include "AtmoDynData.h"
#include "AtmoLiveView.h"
#include "AtmoTools.h"
#include "AtmoExternalCaptureInput.h"
#include "AtmoConfig.h"
#include "AtmoConnection.h"
#include "AtmoClassicConnection.h"


/*****************************************************************************
* Local prototypes
*****************************************************************************/
/* directly to vlc related functions required that the module is accepted */
static int  CreateFilter    ( vlc_object_t * );
static void DestroyFilter   ( vlc_object_t * );
static picture_t * Filter( filter_t *, picture_t *);

/* callback for global variable state pause / continue / stop events */
static void AddStateVariableCallback( filter_t *);
static void DelStateVariableCallback( filter_t *);
static int StateCallback(vlc_object_t *, char const *,
                         vlc_value_t, vlc_value_t, void *);

/* callback for atmo settings variables whose change
   should be immediately realized and applied to output
*/
static void DelAtmoSettingsVariablesCallbacks(filter_t *);
static void AddAtmoSettingsVariablesCallbacks(filter_t *);
static int AtmoSettingsCallback(vlc_object_t *, char const *,
                                vlc_value_t, vlc_value_t, void *);


#if defined(__ATMO_DEBUG__)
static void atmo_parse_crop(char *psz_cropconfig,
                            video_format_t fmt_in,
                            video_format_t fmt_render,
                            int &i_visible_width,
                            int &i_visible_height,
                            int &i_x_offset,
                            int &i_y_offset );
#endif


/* function to shutdown the fade thread which is started on pause*/
static void CheckAndStopFadeThread(filter_t *);

/* extracts a small RGB (BGR) Image from an YUV image */
static void ExtractMiniImage_YUV(filter_sys_t *, picture_t *, uint8_t *);

#if defined(__ATMO_DEBUG__)
void SaveBitmap(filter_sys_t *p_sys, uint8_t *p_pixels, char *psz_filename);
#endif

/*****************************************************************************
* External Prototypes for the AtmoCtrlLib.DLL
*****************************************************************************/
/*
* if effectmode = emLivePicture then the source could be GDI (Screencapture)
* or External - this means another application delivers Pixeldata to AtmoWin
* Clientsoftware through  AtmoCtrlLib.DLL and the COM Api
*/
#define lvsGDI           0
#define lvsExternal      1

#define CLASSIC_ATMO_NUM_ZONES  5


/*
strings for settings menus and hints
*/
#define MODULE_DESCRIPTION N_ ( \
 "This module allows controlling an so called AtmoLight device "\
 "connected to your computer.\n"\
 "AtmoLight is the homegrown version of what Philips calls AmbiLight.\n"\
 "If you need further information feel free to visit us at\n\n"\
 "http://www.vdr-wiki.de/wiki/index.php/Atmo-plugin\n"\
 "http://www.vdr-wiki.de/wiki/index.php/AtmoWin\n\n"\
 "You can find there detailed descriptions on how to build it for yourself "\
 "and where to get the required parts.\n" \
 "You can also have a look at pictures and some movies showing such a device " \
 "in live action.")

#define DRIVER_TEXT            N_("Device type")
#define DRIVER_LONGTEXT        N_("Choose your preferred hardware from " \
                                  "the list, or choose AtmoWin Software " \
                                  "to delegate processing to the external " \
                                  "process - with more options")

static const int pi_device_type_values[] = {
#if defined( _WIN32 )
     0, /* use AtmoWinA.exe userspace driver */
#endif
     1, /* AtmoLight classic */
     2, /* Quattro AtmoLight */
     3, /* DMX Device */
     4, /* MoMoLight device */
     5  /* fnordlicht */
};
static const char *const ppsz_device_type_descriptions[] = {
#if defined( _WIN32 )
        N_("AtmoWin Software"),
#endif
        N_("Classic AtmoLight"),
        N_("Quattro AtmoLight"),
        N_("DMX"),
        N_("MoMoLight"),
        N_("fnordlicht")
};

#define DMX_CHANNELS_TEXT      N_("Count of AtmoLight channels")
#define DMX_CHANNELS_LONGTEXT  N_("How many AtmoLight channels, should be " \
                                  "emulated with that DMX device")
#define DMX_CHBASE_TEXT        N_("DMX address for each channel")
#define DMX_CHBASE_LONGTEXT    N_("Define here the DMX base address for each " \
                                  "channel use , or ; to separate the values")

#define MOMO_CHANNELS_TEXT      N_("Count of channels")
#define MOMO_CHANNELS_LONGTEXT  N_("Depending on your MoMoLight hardware " \
                                   "choose 3 or 4 channels")

#define FNORDLICHT_AMOUNT_TEXT      N_("Count of fnordlicht's")
#define FNORDLICHT_AMOUNT_LONGTEXT  N_("Depending on the amount your " \
                                   "fnordlicht hardware " \
                                   "choose 1 to 254 channels")

#if defined( _WIN32 )
#  define DEFAULT_DEVICE   0
#else
#  define DEFAULT_DEVICE   1
#endif

#if defined( __ATMO_DEBUG__ )
#   define SAVEFRAMES_TEXT     N_("Save Debug Frames")
#   define SAVEFRAMES_LONGTEXT N_("Write every 128th miniframe to a folder.")
#   define FRAMEPATH_TEXT      N_("Debug Frame Folder")
#   define FRAMEPATH_LONGTEXT  N_("The path where the debugframes " \
                                  "should be saved")
#endif

#define WIDTH_TEXT             N_("Extracted Image Width")
#define WIDTH_LONGTEXT         N_("The width of the mini image for " \
                                  "further processing (64 is default)")

#define HEIGHT_TEXT            N_("Extracted Image Height")
#define HEIGHT_LONGTEXT        N_("The height of the mini image for " \
                                  "further processing (48 is default)")

#define SHOW_DOTS_TEXT         N_("Mark analyzed pixels")
#define SHOW_DOTS_LONGTEXT     N_("makes the sample grid visible on screen as "\
                                  "white pixels")

#define PCOLOR_TEXT            N_("Color when paused")
#define PCOLOR_LONGTEXT        N_("Set the color to show if the user " \
                                  "pauses the video. (Have light to get " \
                                  "another beer?)")
#define PCOLOR_RED_TEXT        N_("Pause-Red")
#define PCOLOR_RED_LONGTEXT    N_("Red component of the pause color")
#define PCOLOR_GREEN_TEXT      N_("Pause-Green")
#define PCOLOR_GREEN_LONGTEXT  N_("Green component of the pause color")
#define PCOLOR_BLUE_TEXT       N_("Pause-Blue")
#define PCOLOR_BLUE_LONGTEXT   N_("Blue component of the pause color")
#define FADESTEPS_TEXT         N_("Pause-Fadesteps")
#define FADESTEPS_LONGTEXT     N_("Number of steps to change current color " \
                                  "to pause color (each step takes 40ms)")

#define ECOLOR_RED_TEXT        N_("End-Red")
#define ECOLOR_RED_LONGTEXT    N_("Red component of the shutdown color")
#define ECOLOR_GREEN_TEXT      N_("End-Green")
#define ECOLOR_GREEN_LONGTEXT  N_("Green component of the shutdown color")
#define ECOLOR_BLUE_TEXT       N_("End-Blue")
#define ECOLOR_BLUE_LONGTEXT   N_("Blue component of the shutdown color")
#define EFADESTEPS_TEXT        N_("End-Fadesteps")
#define EFADESTEPS_LONGTEXT  N_("Number of steps to change current color to " \
                             "end color for dimming up the light in cinema " \
                             "style... (each step takes 40ms)")

#define ZONE_TOP_TEXT          N_("Number of zones on top")
#define ZONE_TOP_LONGTEXT      N_("Number of zones on the top of the screen")
#define ZONE_BOTTOM_TEXT       N_("Number of zones on bottom")
#define ZONE_BOTTOM_LONGTEXT   N_("Number of zones on the bottom of the screen")
#define ZONE_LR_TEXT           N_("Zones on left / right side")
#define ZONE_LR_LONGTEXT       N_("left and right side having always the " \
                                  "same number of zones")
#define ZONE_SUMMARY_TEXT      N_("Calculate a average zone")
#define ZONE_SUMMARY_LONGTEXT  N_("it contains the average of all pixels " \
                                  "in the sample image (only useful for " \
                                  "single channel AtmoLight)")


#define USEWHITEADJ_TEXT       N_("Use Software White adjust")
#define USEWHITEADJ_LONGTEXT   N_("Should the buildin driver do a white " \
                                  "adjust or your LED stripes? recommend.")
#define WHITE_RED_TEXT         N_("White Red")
#define WHITE_RED_LONGTEXT     N_("Red value of a pure white on your "\
                                  "LED stripes.")
#define WHITE_GREEN_TEXT       N_("White Green")
#define WHITE_GREEN_LONGTEXT   N_("Green value of a pure white on your "\
                                  "LED stripes.")
#define WHITE_BLUE_TEXT        N_("White Blue")
#define WHITE_BLUE_LONGTEXT    N_("Blue value of a pure white on your "\
                                  "LED stripes.")

#define SERIALDEV_TEXT         N_("Serial Port/Device")
#define SERIALDEV_LONGTEXT   N_("Name of the serial port where the AtmoLight "\
                                "controller is attached to.\n" \
                                "On Windows usually something like COM1 or " \
                                "COM2. On Linux /dev/ttyS01 f.e.")

#define EDGE_TEXT            N_("Edge weightning")
#define EDGE_LONGTEXT        N_("Increasing this value will result in color "\
                                "more depending on the border of the frame.")
#define BRIGHTNESS_TEXT     N_("Brightness")
#define BRIGHTNESS_LONGTEXT N_("Overall brightness of your LED stripes")
#define DARKNESS_TEXT       N_("Darkness limit")
#define DARKNESS_LONGTEXT   N_("Pixels with a saturation lower than this will "\
                               "be ignored. Should be greater than one for "\
                               "letterboxed videos.")
#define HUEWINSIZE_TEXT     N_("Hue windowing")
#define HUEWINSIZE_LONGTEXT N_("Used for statistics.")
#define SATWINSIZE_TEXT     N_("Sat windowing")
#define SATWINSIZE_LONGTEXT N_("Used for statistics.")

#define MEANLENGTH_TEXT     N_("Filter length (ms)")
#define MEANLENGTH_LONGTEXT N_("Time it takes until a color is completely "\
                                "changed. This prevents flickering.")
#define MEANTHRESHOLD_TEXT     N_("Filter threshold")
#define MEANTHRESHOLD_LONGTEXT N_("How much a color has to be changed for an "\
                                  "immediate color change.")
#define MEANPERCENTNEW_TEXT     N_("Filter smoothness (%)")
#define MEANPERCENTNEW_LONGTEXT N_("Filter Smoothness")

#define FILTERMODE_TEXT        N_("Output Color filter mode")
#define FILTERMODE_LONGTEXT    N_("defines the how the output color should " \
                                  "be calculated based on previous color")

static const int pi_filtermode_values[] = {
       (int)afmNoFilter,
       (int)afmCombined,
       (int)afmPercent
};
static const char *const ppsz_filtermode_descriptions[] = {
        N_("No Filtering"),
        N_("Combined"),
        N_("Percent")
};

#define FRAMEDELAY_TEXT       N_("Frame delay (ms)")
#define FRAMEDELAY_LONGTEXT   N_("Helps to get the video output and the light "\
                                 "effects in sync. Values around 20ms should " \
                                 "do the trick.")


#define CHANNEL_0_ASSIGN_TEXT N_("Channel 0: summary")
#define CHANNEL_1_ASSIGN_TEXT N_("Channel 1: left")
#define CHANNEL_2_ASSIGN_TEXT N_("Channel 2: right")
#define CHANNEL_3_ASSIGN_TEXT N_("Channel 3: top")
#define CHANNEL_4_ASSIGN_TEXT N_("Channel 4: bottom")

#define CHANNELASSIGN_LONGTEXT N_("Maps the hardware channel X to logical "\
                                  "zone Y to fix wrong wiring :-)")
static const int pi_zone_assignment_values[] = {
    -1,
     4,
     3,
     1,
     0,
     2
};
static const char *const ppsz_zone_assignment_descriptions[] = {
        N_("disabled"),
        N_("Zone 4:summary"),
        N_("Zone 3:left"),
        N_("Zone 1:right"),
        N_("Zone 0:top"),
        N_("Zone 2:bottom")
};
#define CHANNELS_ASSIGN_TEXT        N_("Channel / Zone Assignment")
#define CHANNELS_ASSIGN_LONGTEXT N_("for devices with more than five " \
                  "channels / zones write down here for each channel " \
                  "the zone number to show and separate the values with " \
                  ", or ; and use -1 to not use some channels. For the " \
                  "classic AtmoLight the sequence 4,3,1,0,2 would set the " \
                  "default channel/zone mapping. " \
                  "Having only two zones on top, and one zone on left and " \
                  "right and no summary zone the mapping for classic " \
                  "AtmoLight would be -1,3,2,1,0")

#define ZONE_0_GRADIENT_TEXT N_("Zone 0: Top gradient")
#define ZONE_1_GRADIENT_TEXT N_("Zone 1: Right gradient")
#define ZONE_2_GRADIENT_TEXT N_("Zone 2: Bottom gradient")
#define ZONE_3_GRADIENT_TEXT N_("Zone 3: Left gradient")
#define ZONE_4_GRADIENT_TEXT N_("Zone 4: Summary gradient")
#define ZONE_X_GRADIENT_LONG_TEXT N_("Defines a small bitmap with 64x48 "\
                                     "pixels, containing a grayscale gradient")

#define GRADIENT_PATH_TEXT      N_("Gradient bitmap searchpath")
#define GRADIENT_PATH_LONGTEXT  N_("Now preferred option to assign gradient "\
    "bitmaps, put them as zone_0.bmp, zone_1.bmp etc. into one folder and "\
    "set the foldername here")

#if defined( _WIN32 )
#   define ATMOWINEXE_TEXT      N_("Filename of AtmoWin*.exe")
#   define ATMOWINEXE_LONGTEXT  N_("if you want the AtmoLight control "\
                                   "software to be launched by VLC, enter the "\
                                   "complete path of AtmoWinA.exe here.")
#endif

#define CFG_PREFIX "atmo-"

/*****************************************************************************
* Module descriptor
*****************************************************************************/
vlc_module_begin ()
set_description( N_("AtmoLight Filter") )
set_help( MODULE_DESCRIPTION )
set_shortname( N_( "AtmoLight" ))
set_category( CAT_VIDEO )
set_subcategory( SUBCAT_VIDEO_VFILTER )

set_capability( "video filter2", 0 )


set_section( N_("Choose Devicetype and Connection" ), 0 )

add_integer( CFG_PREFIX "device", DEFAULT_DEVICE,
            DRIVER_TEXT, DRIVER_LONGTEXT, false )
change_integer_list( pi_device_type_values,
                     ppsz_device_type_descriptions )

#if defined(_WIN32)
add_string(CFG_PREFIX "serialdev", "COM1",
           SERIALDEV_TEXT, SERIALDEV_LONGTEXT, false )
/*
    on win32 the executeable external driver application
    for automatic start if needed
*/
add_loadfile(CFG_PREFIX "atmowinexe", NULL,
             ATMOWINEXE_TEXT, ATMOWINEXE_LONGTEXT, false )
#else
add_string(CFG_PREFIX "serialdev", "/dev/ttyUSB0",
           SERIALDEV_TEXT, SERIALDEV_LONGTEXT, false )
#endif

/*
    color which is showed if you want durring pausing
    your movie ... used for both buildin / external
*/
set_section( N_("Illuminate the room with this color on pause" ), 0 )
add_bool(CFG_PREFIX "usepausecolor", false,
         PCOLOR_TEXT, PCOLOR_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "pcolor-red",   0, 0, 255,
                       PCOLOR_RED_TEXT, PCOLOR_RED_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "pcolor-green", 0, 0, 255,
                       PCOLOR_GREEN_TEXT, PCOLOR_GREEN_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "pcolor-blue",  192, 0, 255,
                       PCOLOR_BLUE_TEXT, PCOLOR_BLUE_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "fadesteps", 50, 1, 250,
                       FADESTEPS_TEXT, FADESTEPS_LONGTEXT, false)

/*
    color which is showed if you finished watching your movie ...
    used for both buildin / external
*/
set_section( N_("Illuminate the room with this color on shutdown" ), 0 )
add_integer_with_range(CFG_PREFIX "ecolor-red",   192, 0, 255,
                       ECOLOR_RED_TEXT,   ECOLOR_RED_LONGTEXT,   false)
add_integer_with_range(CFG_PREFIX "ecolor-green", 192, 0, 255,
                       ECOLOR_GREEN_TEXT, ECOLOR_GREEN_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "ecolor-blue",  192, 0, 255,
                       ECOLOR_BLUE_TEXT,  ECOLOR_BLUE_LONGTEXT,  false)
add_integer_with_range(CFG_PREFIX "efadesteps",    50, 1, 250,
                       EFADESTEPS_TEXT,   EFADESTEPS_LONGTEXT,    false)


set_section( N_("DMX options" ), 0 )
add_integer_with_range(CFG_PREFIX "dmx-channels",   5, 1, 64,
                       DMX_CHANNELS_TEXT, DMX_CHANNELS_LONGTEXT, false)
add_string(CFG_PREFIX "dmx-chbase", "0,3,6,9,12",
                       DMX_CHBASE_TEXT, DMX_CHBASE_LONGTEXT, false )

set_section( N_("MoMoLight options" ), 0 )
add_integer_with_range(CFG_PREFIX "momo-channels",   3, 3, 4,
                       MOMO_CHANNELS_TEXT, MOMO_CHANNELS_LONGTEXT, false)

/* 2,2,4 means 2 is the default value, 1 minimum amount,
   4 maximum amount
*/
set_section( N_("fnordlicht options" ), 0 )
add_integer_with_range(CFG_PREFIX "fnordlicht-amount",   2, 1, 254,
                       FNORDLICHT_AMOUNT_TEXT,
                       FNORDLICHT_AMOUNT_LONGTEXT, false)


/*
  instead of redefining the original AtmoLight zones with gradient
  bitmaps, we can now define the layout of the zones useing these
  parameters - the function with the gradient bitmaps would still
  work (but for most cases its no longer required)

  short description whats this means - f.e. the classic atmo would
  have this layout
  zones-top    = 1  - zone 0
  zones-lr     = 1  - zone 1 und zone 3
  zones-bottom = 1  - zone 2
  zone-summary = true - zone 4
         Z0
   ,------------,
   |            |
 Z3|     Z4     | Z1
   |____________|
         Z2

  the zone numbers will be counted clockwise starting at top / left
  if you want to split the light at the top, without having a bottom zone
  (which is my private config)

  zones-top    = 2  - zone 0, zone 1
  zones-lr     = 1  - zone 2 und zone 3
  zones-bottom = 0
  zone-summary = false

      Z0    Z1
   ,------------,
   |            |
 Z3|            | Z2
   |____________|

*/

set_section( N_("Zone Layout for the build-in Atmo" ), 0 )
add_integer_with_range(CFG_PREFIX "zones-top",   1, 0, 16,
                       ZONE_TOP_TEXT, ZONE_TOP_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "zones-bottom",   1, 0, 16,
                       ZONE_BOTTOM_TEXT, ZONE_BOTTOM_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "zones-lr",   1, 0, 16,
                       ZONE_LR_TEXT, ZONE_LR_LONGTEXT, false)
add_bool(CFG_PREFIX "zone-summary", false,
         ZONE_SUMMARY_TEXT, ZONE_SUMMARY_LONGTEXT, false)

/*
 settings only for the buildin driver (if external driver app is used
 these parameters are ignored.)

 definition of parameters for the buildin filter ...
*/
set_section( N_("Settings for the built-in Live Video Processor only" ), 0 )

add_integer_with_range(CFG_PREFIX "edgeweightning",   3, 1, 30,
                       EDGE_TEXT, EDGE_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "brightness",   100, 50, 300,
                       BRIGHTNESS_TEXT, BRIGHTNESS_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "darknesslimit",   3, 0, 10,
                       DARKNESS_TEXT, DARKNESS_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "huewinsize",   3, 0, 5,
                       HUEWINSIZE_TEXT, HUEWINSIZE_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "satwinsize",   3, 0, 5,
                       SATWINSIZE_TEXT, SATWINSIZE_LONGTEXT, false)

add_integer(CFG_PREFIX "filtermode", (int)afmCombined,
            FILTERMODE_TEXT, FILTERMODE_LONGTEXT, false )

change_integer_list(pi_filtermode_values, ppsz_filtermode_descriptions )

add_integer_with_range(CFG_PREFIX "meanlength",    300, 300, 5000,
                       MEANLENGTH_TEXT, MEANLENGTH_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "meanthreshold",  40, 1, 100,
                       MEANTHRESHOLD_TEXT, MEANTHRESHOLD_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "percentnew", 50, 1, 100,
                      MEANPERCENTNEW_TEXT, MEANPERCENTNEW_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "framedelay", 18, 0, 200,
                       FRAMEDELAY_TEXT, FRAMEDELAY_LONGTEXT, false)

/*
  output channel reordering
*/
set_section( N_("Change channel assignment (fixes wrong wiring)" ), 0 )
add_integer( CFG_PREFIX "channel_0", 4,
            CHANNEL_0_ASSIGN_TEXT, CHANNELASSIGN_LONGTEXT, false )
change_integer_list( pi_zone_assignment_values,
                     ppsz_zone_assignment_descriptions )

add_integer( CFG_PREFIX "channel_1", 3,
            CHANNEL_1_ASSIGN_TEXT, CHANNELASSIGN_LONGTEXT, false )
change_integer_list( pi_zone_assignment_values,
                     ppsz_zone_assignment_descriptions )

add_integer( CFG_PREFIX "channel_2", 1,
            CHANNEL_2_ASSIGN_TEXT, CHANNELASSIGN_LONGTEXT, false )
change_integer_list( pi_zone_assignment_values,
                     ppsz_zone_assignment_descriptions )

add_integer( CFG_PREFIX "channel_3", 0,
            CHANNEL_3_ASSIGN_TEXT, CHANNELASSIGN_LONGTEXT, false )
change_integer_list( pi_zone_assignment_values,
                     ppsz_zone_assignment_descriptions )

add_integer( CFG_PREFIX "channel_4", 2,
            CHANNEL_4_ASSIGN_TEXT, CHANNELASSIGN_LONGTEXT, false )
change_integer_list( pi_zone_assignment_values,
                     ppsz_zone_assignment_descriptions )

add_string(CFG_PREFIX "channels", "",
           CHANNELS_ASSIGN_TEXT, CHANNELS_ASSIGN_LONGTEXT, false )


/*
  LED color white calibration
*/
set_section( N_("Adjust the white light to your LED stripes" ), 0 )
add_bool(CFG_PREFIX "whiteadj", true,
         USEWHITEADJ_TEXT, USEWHITEADJ_LONGTEXT, false)
add_integer_with_range(CFG_PREFIX "white-red",   255, 0, 255,
                       WHITE_RED_TEXT,   WHITE_RED_LONGTEXT,   false)

add_integer_with_range(CFG_PREFIX "white-green", 255, 0, 255,
                       WHITE_GREEN_TEXT, WHITE_GREEN_LONGTEXT, false)

add_integer_with_range(CFG_PREFIX "white-blue",  255, 0, 255,
                       WHITE_BLUE_TEXT,  WHITE_BLUE_LONGTEXT,  false)
/* end of definition of parameter for the buildin filter ... part 1 */


/*
only for buildin (external has own definition) per default the calucation
used linear gradients for assigning a priority to the pixel - depending
how near they are to the border ...for changing this you can create 64x48
Pixel BMP files - which contain your own grayscale... (you can produce funny
effects with this...) the images MUST not compressed, should have 24-bit per
pixel, or a simple 256 color grayscale palette
*/
set_section( N_("Change gradients" ), 0 )
add_loadfile(CFG_PREFIX "gradient_zone_0", NULL,
             ZONE_0_GRADIENT_TEXT, ZONE_X_GRADIENT_LONG_TEXT, true )
add_loadfile(CFG_PREFIX "gradient_zone_1", NULL,
             ZONE_1_GRADIENT_TEXT, ZONE_X_GRADIENT_LONG_TEXT, true )
add_loadfile(CFG_PREFIX "gradient_zone_2", NULL,
             ZONE_2_GRADIENT_TEXT, ZONE_X_GRADIENT_LONG_TEXT, true )
add_loadfile(CFG_PREFIX "gradient_zone_3", NULL,
             ZONE_3_GRADIENT_TEXT, ZONE_X_GRADIENT_LONG_TEXT, true )
add_loadfile(CFG_PREFIX "gradient_zone_4", NULL,
             ZONE_4_GRADIENT_TEXT, ZONE_X_GRADIENT_LONG_TEXT, true )
add_directory(CFG_PREFIX "gradient_path", NULL,
           GRADIENT_PATH_TEXT, GRADIENT_PATH_LONGTEXT, false )

#if defined(__ATMO_DEBUG__)
add_bool(CFG_PREFIX "saveframes", false,
         SAVEFRAMES_TEXT, SAVEFRAMES_LONGTEXT, false)
add_string(CFG_PREFIX "framepath", "",
           FRAMEPATH_TEXT, FRAMEPATH_LONGTEXT, false )
#endif
/*
   may be later if computers gets more power ;-) than now we increase
   the samplesize from which we do the stats for output color calculation
*/
add_integer_with_range(CFG_PREFIX "width",  64, 64, 512,
                       WIDTH_TEXT,  WIDTH_LONGTEXT, true)
add_integer_with_range(CFG_PREFIX "height", 48, 48, 384,
                       HEIGHT_TEXT,  HEIGHT_LONGTEXT, true)
add_bool(CFG_PREFIX "showdots", false,
                   SHOW_DOTS_TEXT, SHOW_DOTS_LONGTEXT, false)
add_shortcut( "atmo" )
set_callbacks( CreateFilter, DestroyFilter  )
vlc_module_end ()


static const char *const ppsz_filter_options[] = {
        "device",

        "serialdev",


        "edgeweightning",
        "brightness",
        "darknesslimit",
        "huewinsize",
        "satwinsize",

        "filtermode",

        "meanlength",
        "meanthreshold",
        "percentnew",
        "framedelay",

        "zones-top",
        "zones-bottom",
        "zones-lr",
        "zone-summary",

        "channel_0",
        "channel_1",
        "channel_2",
        "channel_3",
        "channel_4",
        "channels",

        "whiteadj",
        "white-red",
        "white-green",
        "white-blue",

        "usepausecolor",
        "pcolor-red",
        "pcolor-green",
        "pcolor-blue",
        "fadesteps",

        "ecolor-red",
        "ecolor-green",
        "ecolor-blue",
        "efadesteps",

        "dmx-channels",
        "dmx-chbase",
        "momo-channels",
        "fnordlicht-amount",

#if defined(_WIN32 )
        "atmowinexe",
#endif
#if defined(__ATMO_DEBUG__)
        "saveframes" ,
        "framepath",
#endif
        "width",
        "height",
        "showdots",
        "gradient_zone_0",
        "gradient_zone_1",
        "gradient_zone_2",
        "gradient_zone_3",
        "gradient_zone_4",
        "gradient_path",
        NULL
};


/*****************************************************************************
* fadethread_t: Color Fading Thread
*****************************************************************************
* changes slowly the color of the output if videostream gets paused...
*****************************************************************************
*/
typedef struct
{
    filter_t *p_filter;
    vlc_thread_t thread;
    vlc_atomic_t abort;

    /* tell the thread which color should be the target of fading */
    uint8_t ui_red;
    uint8_t ui_green;
    uint8_t ui_blue;
    /* how many steps should happen until this */
    int i_steps;

} fadethread_t;

static void *FadeToColorThread(void *);


/*****************************************************************************
* filter_sys_t: AtmoLight filter method descriptor
*****************************************************************************
* It describes the AtmoLight specific properties of an video filter.
*****************************************************************************/
struct filter_sys_t
{
    /*
    special for the access of the p_fadethread member all other members
    need no special protection so far!
    */
    vlc_mutex_t filter_lock;

    bool b_enabled;
    int32_t i_AtmoOldEffect;
    bool b_pause_live;
    bool b_show_dots;
    int32_t i_device_type;

    bool b_swap_uv;

    int32_t i_atmo_width;
    int32_t i_atmo_height;
    /* used to disable fadeout if less than 50 frames are processed
       used to avoid long time waiting when switch quickly between
       deinterlaceing modes, where the output filter chains is rebuild
       on each switch
    */
    int32_t i_frames_processed;

#if defined(__ATMO_DEBUG__)
    bool  b_saveframes;
    uint32_t ui_frame_counter;
    char sz_framepath[MAX_PATH];
#endif

    /* light color durring movie pause ... */
    bool  b_usepausecolor;
    uint8_t ui_pausecolor_red;
    uint8_t ui_pausecolor_green;
    uint8_t ui_pausecolor_blue;
    int i_fadesteps;

    /* light color on movie finish ... */
    uint8_t ui_endcolor_red;
    uint8_t ui_endcolor_green;
    uint8_t ui_endcolor_blue;
    int i_endfadesteps;

    fadethread_t *p_fadethread;

    /* Variables for buildin driver only... */

    /* is only present and initialized if the internal driver is used*/
    CAtmoConfig *p_atmo_config;
    /* storage for temporal settings "volatile" */
    CAtmoDynData *p_atmo_dyndata;
    /* initialized for buildin driver with AtmoCreateTransferBuffers */
    VLC_BITMAPINFOHEADER mini_image_format;
    /* is only use buildin driver! */
    uint8_t *p_atmo_transfer_buffer;
    /* end buildin driver */

    /*
    contains the real output size of the video calculated on
    change event of the variable "crop" from vout
    */
    int32_t i_crop_x_offset;
    int32_t i_crop_y_offset;
    int32_t i_crop_width;
    int32_t i_crop_height;

    void (*pf_extract_mini_image) (filter_sys_t *p_sys,
        picture_t *p_inpic,
        uint8_t *p_transfer_dest);

#if defined( _WIN32 )
    /* External Library as wrapper arround COM Stuff */
    HINSTANCE h_AtmoCtrl;
    int32_t (*pf_ctrl_atmo_initialize) (void);
    void (*pf_ctrl_atmo_finalize) (int32_t what);
    int32_t (*pf_ctrl_atmo_switch_effect) (int32_t);
    int32_t (*pf_ctrl_atmo_set_live_source) (int32_t);
    void (*pf_ctrl_atmo_create_transfer_buffers) (int32_t, int32_t,
                                                  int32_t , int32_t);
    uint8_t* (*pf_ctrl_atmo_lock_transfer_buffer) (void);
    void (*pf_ctrl_atmo_send_pixel_data) (void);
    void (*pf_ctrl_atmo_get_image_size)(int32_t *,int32_t *);
#endif
};

/*
initialize previously configured Atmo Light environment
- if internal is enabled try to access the device on the serial port
- if not internal is enabled and we are on win32 try to initialize
the previously loaded DLL ...

Return Values may be: -1 (failed for some reason - filter will be disabled)
1 Ok. lets rock
*/
static int32_t AtmoInitialize(filter_t *p_filter, bool b_for_thread)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if(p_sys->p_atmo_config)
    {
        if(!b_for_thread)
        {
            /* open com port */
            /* setup Output Threads ... */
            msg_Dbg( p_filter, "open atmo device...");
            if(CAtmoTools::RecreateConnection(p_sys->p_atmo_dyndata)
               == ATMO_TRUE)
            {
                return 1;
            } else {
                msg_Err( p_filter,"failed to open atmo device, "\
                                  "some other software/driver may use it?");
            }
        }
#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_initialize)
    {
        /* on win32 with active ctrl dll */
        return p_sys->pf_ctrl_atmo_initialize();
#endif
    }
    return -1;
}

/*
prepare the shutdown of the effect threads,
for build in filter - close the serialport after finishing the threads...
cleanup possible loaded DLL...
*/
static void AtmoFinalize(filter_t *p_filter, int32_t what)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if(p_sys->p_atmo_config)
    {
        if(what == 1)
        {
            CAtmoDynData *p_atmo_dyndata = p_sys->p_atmo_dyndata;
            if(p_atmo_dyndata)
            {
                p_atmo_dyndata->LockCriticalSection();

                CAtmoInput *p_input = p_atmo_dyndata->getLiveInput();
                p_atmo_dyndata->setLiveInput( NULL );
                if(p_input != NULL)
                {
                    p_input->Terminate();
                    delete p_input;
                    msg_Dbg( p_filter, "input thread died peacefully");
                }

                CThread *p_effect_thread = p_atmo_dyndata->getEffectThread();
                p_atmo_dyndata->setEffectThread(NULL);
                if(p_effect_thread != NULL)
                {
                    /*
                    forced the thread to die...
                    and wait for termination of the thread
                    */
                    p_effect_thread->Terminate();
                    delete p_effect_thread;
                    msg_Dbg( p_filter, "effect thread died peacefully");
                }

                CAtmoPacketQueue *p_queue =
                                           p_atmo_dyndata->getLivePacketQueue();
                p_atmo_dyndata->setLivePacketQueue( NULL );
                if(p_queue != NULL)
                {
                   delete p_queue;
                   msg_Dbg( p_filter, "packetqueue removed");
                }

                /*
                close serial port if it is open (all OS specific is inside
                CAtmoSerialConnection implemented / defined)
                */
                CAtmoConnection *p_atmo_connection =
                                 p_atmo_dyndata->getAtmoConnection();
                p_atmo_dyndata->setAtmoConnection(NULL);
                if(p_atmo_connection) {
                    p_atmo_connection->CloseConnection();
                    delete p_atmo_connection;
                }
                p_atmo_dyndata->UnLockCriticalSection();
            }
        }
#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_finalize)
    {
        /* on win32 with active ctrl dll */
        p_sys->pf_ctrl_atmo_finalize(what);
#endif
    }
}

/*
  switch the current light effect to LiveView
*/
static int32_t AtmoSwitchEffect(filter_t *p_filter, int32_t newMode)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    msg_Dbg( p_filter, "AtmoSwitchEffect %d", newMode );

    if(p_sys->p_atmo_config)
    {
       return CAtmoTools::SwitchEffect(p_sys->p_atmo_dyndata, emLivePicture);
#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_switch_effect)
    {
        /* on win32 with active ctrl dll */
        return p_sys->pf_ctrl_atmo_switch_effect( newMode );
#endif
    }
    return emDisabled;
}

/*
set the current live picture source, does only something on win32,
with the external libraries - if the buildin effects are used nothing
happens...
*/
static int32_t AtmoSetLiveSource(filter_t *p_filter, int32_t newSource)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    msg_Dbg( p_filter, "AtmoSetLiveSource %d", newSource );

    if(p_sys->p_atmo_config)
    {
        /*
        buildin driver

        doesnt know different sources so this
        function call would just do nothing special
        in this case
        */
#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_set_live_source)
    {
        /* on win32 with active ctrl dll */
        return p_sys->pf_ctrl_atmo_set_live_source(newSource);
#endif
    }
    return lvsGDI;
}

/*
setup the pixel transferbuffers which is used to transfer pixeldata from
the filter to the effect thread, and possible accross the process
boundaries on win32, with the external DLL
*/
static void AtmoCreateTransferBuffers(filter_t *p_filter,
                                      int32_t FourCC,
                                      int32_t bytePerPixel,
                                      int32_t width,
                                      int32_t height)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if(p_sys->p_atmo_config)
    {
        /*
        we need a buffer where the image is stored (only for transfer
        to the processing thread)
        */
        free( p_sys->p_atmo_transfer_buffer );

        p_sys->p_atmo_transfer_buffer = (uint8_t *)malloc(bytePerPixel *
                                                          width *  height);

        memset(&p_sys->mini_image_format,0,sizeof(VLC_BITMAPINFOHEADER));

        p_sys->mini_image_format.biSize = sizeof(VLC_BITMAPINFOHEADER);
        p_sys->mini_image_format.biWidth = width;
        p_sys->mini_image_format.biHeight = height;
        p_sys->mini_image_format.biBitCount = bytePerPixel*8;
        p_sys->mini_image_format.biCompression = FourCC;

#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_create_transfer_buffers)
    {
        /* on win32 with active ctrl dll */
        p_sys->pf_ctrl_atmo_create_transfer_buffers(FourCC,
            bytePerPixel,
            width,
            height);
#endif
    }
}

/*
acquire the transfer buffer pointer the buildin version only
returns the pointer to the allocated buffer ... the
external version on win32 has to do some COM stuff to lock the
Variant Byte array which is behind the buffer
*/
static uint8_t* AtmoLockTransferBuffer(filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if(p_sys->p_atmo_config)
    {
        return p_sys->p_atmo_transfer_buffer;
#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_lock_transfer_buffer)
    {
        /* on win32 with active ctrl dll */
        return p_sys->pf_ctrl_atmo_lock_transfer_buffer();
#endif
    }
    return NULL;
}

/*
send the content of current pixel buffer got with AtmoLockTransferBuffer
to the processing threads
- build in version - will forward the data to AtmoExternalCaptureInput Thread
- win32 external - will do the same, but across the process boundaries via
COM to the AtmoWinA.exe Process
*/
static void AtmoSendPixelData(filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if(p_sys->p_atmo_config && p_sys->p_atmo_transfer_buffer)
    {
        CAtmoDynData *p_atmo_dyndata = p_sys->p_atmo_dyndata;
        if(p_atmo_dyndata &&
          (p_atmo_dyndata->getLivePictureSource() == lpsExtern))
        {
            /*
            the cast will go Ok because we are inside videolan there is only
            this kind of effect thread implemented!
            */
            CAtmoExternalCaptureInput *p_atmo_external_capture_input_thread =
                (CAtmoExternalCaptureInput *)p_atmo_dyndata->getLiveInput();

            if(p_atmo_external_capture_input_thread)
            {
                /*
                the same as above inside videolan only this single kind of
                input exists so we can cast without further tests!

                this call will do a 1:1 copy of this buffer, and wakeup
                the thread from normal sleeping
                */
                p_atmo_external_capture_input_thread->
                     DeliverNewSourceDataPaket(&p_sys->mini_image_format,
                                               p_sys->p_atmo_transfer_buffer);
            }
        }
#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_send_pixel_data)
    {
        /* on win32 with active ctrl dll */
        p_sys->pf_ctrl_atmo_send_pixel_data();
#endif
    } else
    {
       msg_Warn( p_filter, "AtmoSendPixelData no method");
    }
}

/*
    Shutdown AtmoLight finally - is call from DestroyFilter
    does the cleanup restores the effectmode on the external Software
    (only win32) and possible setup the final light ...
*/
static void Atmo_Shutdown(filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if(p_sys->b_enabled)
    {
        msg_Dbg( p_filter, "shut down atmo!");
        /*
        if there is a still running show pause color thread kill him!
        */
        CheckAndStopFadeThread(p_filter);

        // perpare spawn fadeing thread
        vlc_mutex_lock( &p_sys->filter_lock );

        /*
        fade to end color (in case of external AtmoWin Software
        assume that the static color will equal to this
        one to get a soft change and no flash!
        */
        p_sys->b_pause_live = true;


        p_sys->p_fadethread = (fadethread_t *)calloc( 1, sizeof(fadethread_t) );
        p_sys->p_fadethread->p_filter = p_filter;
        p_sys->p_fadethread->ui_red   = p_sys->ui_endcolor_red;
        p_sys->p_fadethread->ui_green = p_sys->ui_endcolor_green;
        p_sys->p_fadethread->ui_blue  = p_sys->ui_endcolor_blue;
        if(p_sys->i_frames_processed < 50)
          p_sys->p_fadethread->i_steps  = 1;
        else
          p_sys->p_fadethread->i_steps  = p_sys->i_endfadesteps;
        vlc_atomic_set(&p_sys->p_fadethread->abort, 0);

        if( vlc_clone( &p_sys->p_fadethread->thread,
                       FadeToColorThread,
                       p_sys->p_fadethread,
                       VLC_THREAD_PRIORITY_LOW ) )
        {
            msg_Err( p_filter, "cannot create FadeToColorThread" );
            free( p_sys->p_fadethread );
            p_sys->p_fadethread = NULL;
            vlc_mutex_unlock( &p_sys->filter_lock );

        } else {

            vlc_mutex_unlock( &p_sys->filter_lock );

            /* wait for the thread... */
            vlc_join(p_sys->p_fadethread->thread, NULL);

            free(p_sys->p_fadethread);

            p_sys->p_fadethread = NULL;
        }

        /*
           the following happens only useing the
           external AtmoWin Device Software
        */
        if( !p_sys->p_atmo_config )
        {
           if(p_sys->i_AtmoOldEffect != emLivePicture)
              AtmoSwitchEffect( p_filter, p_sys->i_AtmoOldEffect);
           else
              AtmoSetLiveSource( p_filter, lvsGDI );
        }

        /* close device connection etc. */
        AtmoFinalize(p_filter, 1);

        /* disable filter method .. */
        p_sys->b_enabled = false;
    }
}

/*
depending on mode setup imagesize to 64x48(classic), or defined
resolution of external atmowin.exe on windows
*/
static void Atmo_SetupImageSize(filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    /*
       size of extracted image by default 64x48 (other imagesizes are
       currently ignored by AtmoWin)
    */
    p_sys->i_atmo_width  = var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "width");
    p_sys->i_atmo_height = var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "height");

    if(p_sys->p_atmo_config)
    {
#if defined(_WIN32)
    } else if(p_sys->pf_ctrl_atmo_get_image_size)
    {
        /* on win32 with active ctrl dll */
        p_sys->pf_ctrl_atmo_get_image_size( &p_sys->i_atmo_width,
                                            &p_sys->i_atmo_height );
#endif
    }

    msg_Dbg(p_filter,"sample image size %d * %d pixels", p_sys->i_atmo_width,
        p_sys->i_atmo_height);
}

/*
initialize the zone and channel mapping for the buildin atmolight adapter
*/
static void Atmo_SetupBuildZones(filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    p_sys->p_atmo_dyndata->LockCriticalSection();

    CAtmoConfig *p_atmo_config = p_sys->p_atmo_config;


    CAtmoChannelAssignment *p_channel_assignment =
                            p_atmo_config->getChannelAssignment(0);

    // channel 0 - zone 4
    p_channel_assignment->setZoneIndex( 0, var_CreateGetIntegerCommand(
                                        p_filter, CFG_PREFIX "channel_0")
                                        );

    // channel 1 - zone 3
    p_channel_assignment->setZoneIndex( 1, var_CreateGetIntegerCommand(
                                        p_filter, CFG_PREFIX "channel_1")
                                        );

    // channel 2 - zone 1
    p_channel_assignment->setZoneIndex( 2, var_CreateGetIntegerCommand(
                                        p_filter, CFG_PREFIX "channel_2")
                                        );

    // channel 3 - zone 0
    p_channel_assignment->setZoneIndex( 3, var_CreateGetIntegerCommand(
                                        p_filter, CFG_PREFIX "channel_3")
                                        );

    // channel 4 - zone 2
    p_channel_assignment->setZoneIndex( 4, var_CreateGetIntegerCommand(
                                        p_filter, CFG_PREFIX "channel_4")
                                        );

    char *psz_channels = var_CreateGetStringCommand(
              p_filter,
              CFG_PREFIX "channels"
            );
    if( !EMPTY_STR(psz_channels) )
    {
        msg_Dbg( p_filter, "deal with new zone mapping %s", psz_channels );
        int channel = 0;
        char *psz_temp = psz_channels;
        char *psz_start = psz_temp;
        while( *psz_temp )
        {
            if(*psz_temp == ',' || *psz_temp == ';')
            {
                *psz_temp = 0;
                if(*psz_start)
                {
                    int zone = atoi( psz_start );
                    if( zone < -1 ||
                        zone >= p_channel_assignment->getSize()) {
                         msg_Warn( p_filter, "Zone %d out of range -1..%d",
                                zone, p_channel_assignment->getSize()-1 );
                    } else {
                        p_channel_assignment->setZoneIndex( channel, zone );
                        channel++;
                    }
                }
                psz_start = psz_temp;
                psz_start++;
            }

            psz_temp++;
        }

        /*
          process the rest of the string
        */
        if( *psz_start && !*psz_temp )
        {
            int zone = atoi( psz_start );
            if( zone < -1 ||
                zone >= p_channel_assignment->getSize()) {
                msg_Warn( p_filter, "Zone %d out of range -1..%d",
                            zone, p_channel_assignment->getSize()-1 );
            } else {
                p_channel_assignment->setZoneIndex( channel, zone );
            }
        }
    }
    free( psz_channels );

    for(int i=0;i< p_channel_assignment->getSize() ;i++)
        msg_Info( p_filter, "map zone %d to hardware channel %d",
        p_channel_assignment->getZoneIndex( i ),
        i
        );
    p_sys->p_atmo_dyndata->getAtmoConnection()
         ->SetChannelAssignment( p_channel_assignment );





    /*
      calculate the default gradients for each zone!
      depending on the zone layout set before, this now
      supports also multiple gradients on each side
      (older versions could do this only with external
      gradient bitmaps)
    */
    p_sys->p_atmo_dyndata->CalculateDefaultZones();


    /*
      first try to load the old style defined gradient bitmaps
      this could only be done for the first five zones
      - should be deprecated -
    */
    CAtmoZoneDefinition *p_zone;
    char psz_gradient_var_name[30];
    char *psz_gradient_file;
    for(int i=0;i<CLASSIC_ATMO_NUM_ZONES;i++)
    {
        sprintf(psz_gradient_var_name, CFG_PREFIX "gradient_zone_%d", i);
        psz_gradient_file = var_CreateGetStringCommand(
            p_filter,
            psz_gradient_var_name
            );
        if( !EMPTY_STR(psz_gradient_file) )
        {
            msg_Dbg( p_filter, "loading gradientfile %s for "\
                                "zone %d", psz_gradient_file, i);

            p_zone = p_atmo_config->getZoneDefinition(i);
            if( p_zone )
            {
                int i_res = p_zone->LoadGradientFromBitmap(psz_gradient_file);

                if(i_res != ATMO_LOAD_GRADIENT_OK)
                {
                    msg_Err( p_filter,"failed to load gradient '%s' with "\
                                    "error %d",psz_gradient_file,i_res);
                }
            }
        }
        free( psz_gradient_file );
    }


    /*
      the new approach try to load a gradient bitmap for each zone
      from a previously defined folder containing
      zone_0.bmp
      zone_1.bmp
      zone_2.bmp etc.
    */
    char *psz_gradient_path = var_CreateGetStringCommand(
              p_filter,
              CFG_PREFIX "gradient_path"
            );
    if( EMPTY_STR(psz_gradient_path) )
    {
        char *psz_file_name = (char *)malloc( strlen(psz_gradient_path) + 16 );
        assert( psz_file_name );

        for(int i=0; i < p_atmo_config->getZoneCount(); i++ )
        {
            p_zone = p_atmo_config->getZoneDefinition(i);

            if( p_zone )
            {
                sprintf(psz_file_name, "%s%szone_%d.bmp",
                                            psz_gradient_path, DIR_SEP, i );

                int i_res = p_zone->LoadGradientFromBitmap( psz_file_name );

                if( i_res == ATMO_LOAD_GRADIENT_OK )
                {
                msg_Dbg( p_filter, "loaded gradientfile %s for "\
                                   "zone %d", psz_file_name, i);
                }

                if( (i_res != ATMO_LOAD_GRADIENT_OK) &&
                    (i_res != ATMO_LOAD_GRADIENT_FILENOTFOND) )
                {
                    msg_Err( p_filter,"failed to load gradient '%s' with "\
                                    "error %d",psz_file_name,i_res);
                }
            }
        }

        free( psz_file_name );
    }
    free( psz_gradient_path );


    p_sys->p_atmo_dyndata->UnLockCriticalSection();

}

static void Atmo_SetupConfig(filter_t *p_filter, CAtmoConfig *p_atmo_config)
{
    /*
       figuring out the device ports (com-ports, ttys)
    */
    char *psz_serialdev = var_CreateGetStringCommand( p_filter,
                                                      CFG_PREFIX "serialdev" );
    char *psz_temp = psz_serialdev;

    if( !EMPTY_STR(psz_serialdev) )
    {
        char *psz_token;
        int i_port = 0;
        int i;
        int j;

        msg_Dbg( p_filter, "use port(s) %s",psz_serialdev);

        /*
          psz_serialdev - may contain up to 4 COM ports for the quattro device
          the quattro device is just hack of useing 4 classic devices as one
          logical device - thanks that usb-com-ports exists :)
          as Seperator I defined , or ; with the hope that these
          characters are never part of a device name
        */
        while( (psz_token = strsep(&psz_temp, ",;")) != NULL && i_port < 4 )
        {
            /*
              psz_token may contain spaces we have to trim away
            */
            i = 0;
            j = 0;
            /*
              find first none space in string
            */
            while( psz_token[i] == 32 ) i++;
            /*
              contains string only spaces or is empty? skip it
            */
            if( !psz_token[i] )
                continue;

            /*
              trim
            */
            while( psz_token[i] && psz_token[i] != 32 )
                psz_token[ j++ ] = psz_token[ i++ ];
            psz_token[j++] = 0;

            msg_Dbg( p_filter, "Serial Device [%d]: %s", i_port, psz_token );

            p_atmo_config->setSerialDevice( i_port, psz_token );

            i_port++;
        }
    }
    else
    {
       msg_Err(p_filter,"no serial devicename(s) set");
    }
    free( psz_serialdev );

    /*
      configuration of light source layout arround the display
    */
    p_atmo_config->setZonesTopCount(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "zones-top")
        );
    p_atmo_config->setZonesBottomCount(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "zones-bottom")
        );
    p_atmo_config->setZonesLRCount(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "zones-lr")
        );
    p_atmo_config->setZoneSummary(
        var_CreateGetBoolCommand( p_filter, CFG_PREFIX "zone-summary")
        );


    p_atmo_config->setLiveViewFilterMode(
        (AtmoFilterMode)var_CreateGetIntegerCommand( p_filter,
                                                CFG_PREFIX "filtermode")
        );

    p_atmo_config->setLiveViewFilter_PercentNew(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "percentnew")
        );
    p_atmo_config->setLiveViewFilter_MeanLength(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "meanlength")
        );
    p_atmo_config->setLiveViewFilter_MeanThreshold(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "meanthreshold")
        );

    p_atmo_config->setLiveView_EdgeWeighting(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "edgeweightning")
        );
    p_atmo_config->setLiveView_BrightCorrect(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "brightness")
        );
    p_atmo_config->setLiveView_DarknessLimit(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "darknesslimit")
        );
    p_atmo_config->setLiveView_HueWinSize(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "huewinsize")
        );
    p_atmo_config->setLiveView_SatWinSize(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "satwinsize")
        );

    /* currently not required inside vlc */
    p_atmo_config->setLiveView_WidescreenMode( 0 );

    p_atmo_config->setLiveView_FrameDelay(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "framedelay")
        );


    p_atmo_config->setUseSoftwareWhiteAdj(
        var_CreateGetBoolCommand( p_filter, CFG_PREFIX "whiteadj")
        );
    p_atmo_config->setWhiteAdjustment_Red(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "white-red")
        );
    p_atmo_config->setWhiteAdjustment_Green(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "white-green")
        );
    p_atmo_config->setWhiteAdjustment_Blue(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "white-blue")
        );

    /*
      settings for DMX device only
    */
    p_atmo_config->setDMX_RGB_Channels(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "dmx-channels")
        );

    char *psz_chbase = var_CreateGetStringCommand( p_filter,
                                                   CFG_PREFIX "dmx-chbase" );
    if( !EMPTY_STR(psz_chbase) )
        p_atmo_config->setDMX_BaseChannels( psz_chbase );

    free( psz_chbase );

    /*
      momolight options
    */
    p_atmo_config->setMoMo_Channels(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "momo-channels")
       );

    /*
      fnordlicht options
    */
    p_atmo_config->setFnordlicht_Amount(
        var_CreateGetIntegerCommand( p_filter, CFG_PREFIX "fnordlicht-amount")
       );

}


/*
initialize the filter_sys_t structure with the data from the settings
variables - if the external filter on win32 is enabled try loading the DLL,
if this fails fallback to the buildin software
*/
static void Atmo_SetupParameters(filter_t *p_filter)
{
    filter_sys_t *p_sys =  p_filter->p_sys;


    /* default filter disabled until DLL loaded and Init Success!*/
    p_sys->b_enabled             = false;

    /* setup default mini image size (may be later a user option) */
    p_sys->i_atmo_width          = 64;
    p_sys->i_atmo_height         = 48;

    p_sys->i_device_type = var_CreateGetIntegerCommand( p_filter,
                                                        CFG_PREFIX "device");

    /*
      i_device_type
       0 => use AtmoWin Software (only win32)
       1 => use AtmoClassicConnection (direct)
       2 => use AtmoMultiConnection (direct up to four serial ports required)
       3 => use AtmoDmxConnection (simple serial DMX Device up to 255 channels)
    */


#if defined(_WIN32)
    /*
    only on _WIN32 the user has the choice between
    internal driver and external
    */

    if(p_sys->i_device_type == 0) {

        /* Load the Com Wrapper Library (source available) */
        p_sys->h_AtmoCtrl = LoadLibraryA("AtmoCtrlLib.dll");
        if(p_sys->h_AtmoCtrl == NULL)
        {
            /*
              be clever if the location of atmowin.exe is set
              try to load the dll from the same folder :-)
            */
            char *psz_path = var_CreateGetStringCommand( p_filter,
                                               CFG_PREFIX "atmowinexe" );
            if( !EMPTY_STR(psz_path) )
            {
                char *psz_bs = strrchr( psz_path , '\\');
                if( psz_bs )
                {
                    *psz_bs = 0;
                    /*
                      now format a new dll filename with complete path
                    */
                    char *psz_dllname = NULL;
                    asprintf( &psz_dllname, "%s\\AtmoCtrlLib.dll", psz_path );
                    if( psz_dllname )
                    {
                        msg_Dbg( p_filter, "Try Loading '%s'", psz_dllname );
                        TCHAR* ptsz_dllname = ToT(psz_dllname);
                        p_sys->h_AtmoCtrl = LoadLibrary( ptsz_dllname );
                        free(ptsz_dllname);
                    }
                    free( psz_dllname );
                }
            }
            free( psz_path );
        }


        if(p_sys->h_AtmoCtrl != NULL)
        {
            msg_Dbg( p_filter, "Load Library ok!");

            /* importing all required functions I hope*/
            p_sys->pf_ctrl_atmo_initialize =
                (int32_t (*)(void))GetProcAddress(p_sys->h_AtmoCtrl,
                            "AtmoInitialize");
            if(!p_sys->pf_ctrl_atmo_initialize)
                msg_Err( p_filter, "export AtmoInitialize missing.");

            p_sys->pf_ctrl_atmo_finalize =
                (void (*)(int32_t))GetProcAddress(p_sys->h_AtmoCtrl,
                            "AtmoFinalize");
            if(!p_sys->pf_ctrl_atmo_finalize)
                msg_Err( p_filter, "export AtmoFinalize missing.");

            p_sys->pf_ctrl_atmo_switch_effect =
                (int32_t(*)(int32_t))GetProcAddress(p_sys->h_AtmoCtrl,
                            "AtmoSwitchEffect");
            if(!p_sys->pf_ctrl_atmo_switch_effect)
                msg_Err( p_filter, "export AtmoSwitchEffect missing.");

            p_sys->pf_ctrl_atmo_set_live_source =
                (int32_t(*)(int32_t))GetProcAddress(p_sys->h_AtmoCtrl,
                            "AtmoSetLiveSource");
            if(!p_sys->pf_ctrl_atmo_set_live_source)
                msg_Err( p_filter, "export AtmoSetLiveSource missing.");

            p_sys->pf_ctrl_atmo_create_transfer_buffers =
                (void (*)(int32_t, int32_t, int32_t , int32_t))
                    GetProcAddress(p_sys->h_AtmoCtrl,"AtmoCreateTransferBuffers");
            if(!p_sys->pf_ctrl_atmo_create_transfer_buffers)
                msg_Err( p_filter, "export AtmoCreateTransferBuffers missing.");

            p_sys->pf_ctrl_atmo_lock_transfer_buffer=
                (uint8_t*(*) (void))GetProcAddress(p_sys->h_AtmoCtrl,
                            "AtmoLockTransferBuffer");
            if(!p_sys->pf_ctrl_atmo_lock_transfer_buffer)
                msg_Err( p_filter, "export AtmoLockTransferBuffer missing.");

            p_sys->pf_ctrl_atmo_send_pixel_data =
                (void (*)(void))GetProcAddress(p_sys->h_AtmoCtrl,
                            "AtmoSendPixelData");
            if(!p_sys->pf_ctrl_atmo_send_pixel_data)
                msg_Err( p_filter, "export AtmoSendPixelData missing.");

            p_sys->pf_ctrl_atmo_get_image_size =
                (void (*)(int32_t*,int32_t*))GetProcAddress(p_sys->h_AtmoCtrl,
                            "AtmoWinGetImageSize");
            if(!p_sys->pf_ctrl_atmo_get_image_size)
                msg_Err( p_filter, "export AtmoWinGetImageSize missing.");

        } else {
            /* the DLL is missing try internal filter ...*/
            msg_Warn( p_filter,
                "AtmoCtrlLib.dll missing fallback to internal atmo classic driver");
            p_sys->i_device_type = 1;
        }
    }
#endif

    if(p_sys->i_device_type >= 1) {
        msg_Dbg( p_filter, "try use buildin driver %d ", p_sys->i_device_type);
        /*
        now we have to read a lof of options from the config dialog
        most important the serial device if not set ... we can skip
        the rest and disable the filter...
        */

        p_sys->p_atmo_config = new CAtmoConfig();

        p_sys->p_atmo_dyndata = new CAtmoDynData(
                   (vlc_object_t *)p_filter,
                   p_sys->p_atmo_config
        );

        Atmo_SetupConfig( p_filter, p_sys->p_atmo_config );
        switch(p_sys->i_device_type)
        {
            case 1:
                p_sys->p_atmo_config->setConnectionType( actClassicAtmo );
                break;

            case 2:
                p_sys->p_atmo_config->setConnectionType( actMultiAtmo );
                break;

            case 3:
                p_sys->p_atmo_config->setConnectionType( actDMX );
                break;

            case 4:
                p_sys->p_atmo_config->setConnectionType( actMoMoLight );
                break;

            case 5:
                p_sys->p_atmo_config->setConnectionType( actFnordlicht );
                break;

            default:
                msg_Warn( p_filter, "invalid device type %d found",
                                    p_sys->i_device_type );
        }

        msg_Dbg( p_filter, "buildin driver config set");

    }

    switch( p_filter->fmt_in.video.i_chroma )
    {
    case VLC_CODEC_I420:
        p_sys->pf_extract_mini_image = ExtractMiniImage_YUV;
        p_sys->b_swap_uv = false;
        break;
    case VLC_CODEC_YV12:
        p_sys->pf_extract_mini_image = ExtractMiniImage_YUV;
        p_sys->b_swap_uv = true;
        break;
    default:
        msg_Warn( p_filter, "InitFilter-unsupported chroma: %4.4s",
                            (char *)&p_filter->fmt_in.video.i_chroma);
        p_sys->pf_extract_mini_image = NULL;
    }

    /*
    for debugging purpose show the samplinggrid on each frame as
    white dots
    */
    p_sys->b_show_dots = var_CreateGetBoolCommand( p_filter,
        CFG_PREFIX "showdots"
        );

#if defined(__ATMO_DEBUG__)
    /* save debug images to a folder as Bitmap files ? */
    p_sys->b_saveframes  = var_CreateGetBoolCommand( p_filter,
        CFG_PREFIX "saveframes"
        );
    msg_Dbg(p_filter,"saveframes = %d", (int)p_sys->b_saveframes);

    /*
    read debug image folder from config
    */
    psz_path = var_CreateGetStringCommand( p_filter, CFG_PREFIX "framepath" );
    if(psz_path != NULL)
    {
        strcpy(p_sys->sz_framepath, psz_path);
#if defined( _WIN32 ) || defined( __OS2__ )
        size_t i_strlen = strlen(p_sys->sz_framepath);
        if((i_strlen>0) && (p_sys->sz_framepath[i_strlen-1] != '\\'))
        {
            p_sys->sz_framepath[i_strlen] = '\\';
            p_sys->sz_framepath[i_strlen+1] = 0;
        }
#endif
        free(psz_path);
    }
    msg_Dbg(p_filter,"saveframesfolder %s",p_sys->sz_framepath);
#endif


    /*
    because atmowin could also be used for lighten up the room - I think if you
    pause the video it would be useful to get a little bit more light into to
    your living room? - instead switching on a lamp?
    */
    p_sys->b_usepausecolor = var_CreateGetBoolCommand( p_filter,
        CFG_PREFIX "usepausecolor" );
    p_sys->ui_pausecolor_red = (uint8_t)var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "pcolor-red");
    p_sys->ui_pausecolor_green = (uint8_t)var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "pcolor-green");
    p_sys->ui_pausecolor_blue = (uint8_t)var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "pcolor-blue");
    p_sys->i_fadesteps = var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "fadesteps");
    if(p_sys->i_fadesteps < 1)
        p_sys->i_fadesteps = 1;
    msg_Dbg(p_filter,"use pause color %d, RGB: %d, %d, %d, Fadesteps: %d",
        (int)p_sys->b_usepausecolor,
        p_sys->ui_pausecolor_red,
        p_sys->ui_pausecolor_green,
        p_sys->ui_pausecolor_blue,
        p_sys->i_fadesteps);

    /*
    this color is use on shutdown of the filter - the define the
    final light after playback... may be used to dim up the light -
    how it happens in the cinema...
    */
    p_sys->ui_endcolor_red = (uint8_t)var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "ecolor-red");
    p_sys->ui_endcolor_green = (uint8_t)var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "ecolor-green");
    p_sys->ui_endcolor_blue = (uint8_t)var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "ecolor-blue");
    p_sys->i_endfadesteps = var_CreateGetIntegerCommand( p_filter,
        CFG_PREFIX "efadesteps");
    if(p_sys->i_endfadesteps < 1)
        p_sys->i_endfadesteps = 1;
    msg_Dbg(p_filter,"use ende color RGB: %d, %d, %d, Fadesteps: %d",
        p_sys->ui_endcolor_red,
        p_sys->ui_endcolor_green,
        p_sys->ui_endcolor_blue,
        p_sys->i_endfadesteps);



    /*
      if the external DLL was loaded successfully call AtmoInitialize -
      (must be done for each thread where you want to use AtmoLight!)
    */
    int i = AtmoInitialize(p_filter, false);

#if defined( _WIN32 )
    if((i != 1) && (p_sys->i_device_type == 0))
    {
        /*
          COM Server for AtmoLight not running ?
          if the exe path is configured try to start the "userspace" driver
        */
        char *psz_path = var_CreateGetStringCommand( p_filter,
                                               CFG_PREFIX "atmowinexe" );
        LPTSTR ptsz_path = ToT(psz_path);
        if(psz_path != NULL)
        {
            STARTUPINFO startupinfo;
            PROCESS_INFORMATION pinfo;
            memset(&startupinfo, 0, sizeof(STARTUPINFO));
            startupinfo.cb = sizeof(STARTUPINFO);
            if(CreateProcess(ptsz_path, NULL, NULL, NULL,
                FALSE, 0, NULL, NULL, &startupinfo, &pinfo) == TRUE)
            {
                msg_Dbg(p_filter,"launched AtmoWin from %s", psz_path);
                WaitForInputIdle(pinfo.hProcess, 5000);
                /*
                  retry to initialize the library COM ... functionality
                  after the server was launched
                */
                i = AtmoInitialize(p_filter, false);
            } else {
                msg_Err(p_filter,"failed to launch AtmoWin from %s", psz_path);
            }
            free(psz_path);
            free(ptsz_path);
        }
    }
#endif

    if(i == 1) /* Init Atmolight success... */
    {
        msg_Dbg( p_filter, "AtmoInitialize Ok!");
        /*
        configure
           p_sys->i_atmo_width and p_sys->i_atmo_height
           if the external AtmoWinA.exe is used, it may require
           a other sample image size than 64 x 48
           (this overrides the settings of the filter)
        */
        Atmo_SetupImageSize( p_filter );


        if( p_sys->i_device_type >= 1 )
        {
           /*
             AtmoConnection class initialized now we can initialize
             the default zone and channel mappings
           */
           Atmo_SetupBuildZones( p_filter );
        }

        /* Setup Transferbuffers for 64 x 48 , RGB with 32bit Per Pixel */
        AtmoCreateTransferBuffers(p_filter, BI_RGB, 4,
            p_sys->i_atmo_width,
            p_sys->i_atmo_height
            );

        /* say the userspace driver that a live mode should be activated
        the functions returns the old mode for later restore!
        - the buildin driver launches the live view thread in that case
        */
        p_sys->i_AtmoOldEffect = AtmoSwitchEffect(p_filter, emLivePicture);

        /*
        live view can have two differnt source the AtmoWinA
        internal GDI Screencapture and the external one - which we
        need here...
        */
        AtmoSetLiveSource(p_filter, lvsExternal);

        /* enable other parts only if everything is fine */
        p_sys->b_enabled = true;

        msg_Dbg( p_filter, "Atmo Filter Enabled Ok!");
    }

}


/*****************************************************************************
* CreateFilter: allocates AtmoLight video thread output method
*****************************************************************************
* This function allocates and initializes a AtmoLight vout method.
*****************************************************************************/
static int CreateFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys;

    /* Allocate structure */
    p_sys = (filter_sys_t *)malloc( sizeof( filter_sys_t ) );
    p_filter->p_sys = p_sys;
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;
    /* set all entries to zero */
    memset(p_sys, 0, sizeof( filter_sys_t ));
    vlc_mutex_init( &p_sys->filter_lock );

    msg_Dbg( p_filter, "Create Atmo Filter");

    /* further Setup Function pointers for videolan for calling my filter */
    p_filter->pf_video_filter = Filter;

    config_ChainParse( p_filter, CFG_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    AddStateVariableCallback(p_filter);

    AddAtmoSettingsVariablesCallbacks(p_filter);

    Atmo_SetupParameters(p_filter);


    return VLC_SUCCESS;
}



/*****************************************************************************
* DestroyFilter: destroy AtmoLight video thread output method
*****************************************************************************
* Terminate an output method created by CreateFilter
*****************************************************************************/

static void DestroyFilter( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys =  p_filter->p_sys;

    msg_Dbg( p_filter, "Destroy Atmo Filter");

    DelStateVariableCallback(p_filter);

    DelAtmoSettingsVariablesCallbacks(p_filter);

    Atmo_Shutdown(p_filter);

#if defined( _WIN32 )
    if(p_sys->h_AtmoCtrl != NULL)
    {
        FreeLibrary(p_sys->h_AtmoCtrl);
    }
#endif

    delete p_sys->p_atmo_dyndata;
    delete p_sys->p_atmo_config;

    vlc_mutex_destroy( &p_sys->filter_lock );

    free( p_sys );
}


/*
function stolen from some other videolan source filter ;-)
for the moment RGB is OK... but better would be a direct transformation
from YUV --> HSV
*/
static inline void yuv_to_rgb( uint8_t *r, uint8_t *g, uint8_t *b,
                              uint8_t y1, uint8_t u1, uint8_t v1 )
{
    /* macros used for YUV pixel conversions */
#   define SCALEBITS 10
#   define ONE_HALF  (1 << (SCALEBITS - 1))
#   define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))
#   define CLAMP( x ) (((x) > 255) ? 255 : ((x) < 0) ? 0 : (x));

    int y, cb, cr, r_add, g_add, b_add;

    cb = u1 - 128;
    cr = v1 - 128;
    r_add = FIX(1.40200*255.0/224.0) * cr + ONE_HALF;
    g_add = - FIX(0.34414*255.0/224.0) * cb
        - FIX(0.71414*255.0/224.0) * cr + ONE_HALF;
    b_add = FIX(1.77200*255.0/224.0) * cb + ONE_HALF;
    y = (y1 - 16) * FIX(255.0/219.0);
    *r = CLAMP((y + r_add) >> SCALEBITS);
    *g = CLAMP((y + g_add) >> SCALEBITS);
    *b = CLAMP((y + b_add) >> SCALEBITS);
}
/******************************************************************************
* ExtractMiniImage_YUV: extract a small image from the picture as 24-bit RGB
*******************************************************************************
* p_sys is a pointer to
* p_inpic is the source frame
* p_transfer_dest is the target buffer for the picture must be big enough!
* (in win32 environment this buffer comes from the external DLL where it is
* create as "variant array" and returned through the AtmoLockTransferbuffer
*/
static void ExtractMiniImage_YUV(filter_sys_t *p_sys,
                                 picture_t *p_inpic,
                                 uint8_t *p_transfer_dest)
{
    int i_col;
    int i_row;
    uint8_t *p_src_y;
    uint8_t *p_src_u;
    uint8_t *p_src_v;
    uint8_t *p_rgb_dst_line_red;
    uint8_t *p_rgb_dst_line_green;
    uint8_t *p_rgb_dst_line_blue;
    int i_xpos_y;
    int i_xpos_u;
    int i_xpos_v;

    /* calcute Pointers for Storage of B G R (A) */
    p_rgb_dst_line_blue      = p_transfer_dest;
    p_rgb_dst_line_green     = p_transfer_dest + 1;
    p_rgb_dst_line_red       = p_transfer_dest + 2 ;

    int i_row_count = p_sys->i_atmo_height + 1;
    int i_col_count = p_sys->i_atmo_width + 1;
    int i_y_row,i_u_row,i_v_row,i_pixel_row;
    int i_pixel_col;


    /*  these two ugly loops extract the small image - goes it faster? how?
    the loops are so designed that there is a small border around the extracted
    image so we won't get column and row - zero from the frame, and not the most
    right and bottom pixels --- which may be clipped on computers useing TV out
    - through overscan!

    TODO: try to find out if the output is clipped through VLC - and try here
    to ingore the clipped away area for a better result!

    TODO: performance improvement in InitFilter percalculated the offsets of
    the lines inside the planes so I can save (i_row_count * 3) 2xMUL and
    one time DIV the same could be done for the inner loop I think...
    */
    for(i_row = 1; i_row < i_row_count; i_row++)
    {
        // calcute the current Lines in the source planes for this outputrow
        /*  Adresscalcuation  pointer to plane  Length of one pixelrow in bytes
        calculate row now number
        */
        /*
           p_inpic->format? transform Pixel row into row of plane...
           how? simple? fast? good?
        */

        /* compute the source pixel row and respect the active cropping */
        i_pixel_row = (i_row * p_sys->i_crop_height) / i_row_count
            + p_sys->i_crop_y_offset;

        /*
        trans for these Pixel row into the row of each plane ..
        because planesize can differ from image size
        */
        i_y_row = (i_pixel_row * p_inpic->p[Y_PLANE].i_visible_lines) /
            p_inpic->format.i_visible_height;

        i_u_row = (i_pixel_row * p_inpic->p[U_PLANE].i_visible_lines) /
            p_inpic->format.i_visible_height;

        i_v_row = (i_pixel_row * p_inpic->p[V_PLANE].i_visible_lines) /
            p_inpic->format.i_visible_height;

        /* calculate  the pointers to the pixeldata for this row
           in each plane
        */
        p_src_y = p_inpic->p[Y_PLANE].p_pixels +
            p_inpic->p[Y_PLANE].i_pitch * i_y_row;
        p_src_u = p_inpic->p[U_PLANE].p_pixels +
            p_inpic->p[U_PLANE].i_pitch * i_u_row;
        p_src_v = p_inpic->p[V_PLANE].p_pixels +
            p_inpic->p[V_PLANE].i_pitch * i_v_row;

        if(p_sys->b_swap_uv)
        {
          /*
           swap u and v plane for YV12 images
          */
          uint8_t *p_temp_plane = p_src_u;
          p_src_u = p_src_v;
          p_src_v = p_temp_plane;
        }

        for(i_col = 1; i_col < i_col_count; i_col++)
        {
            i_pixel_col = (i_col * p_sys->i_crop_width) / i_col_count +
                p_sys->i_crop_x_offset;
            /*
            trans for these Pixel row into the row of each plane ..
            because planesize can differ from image size
            */
            i_xpos_y = (i_pixel_col * p_inpic->p[Y_PLANE].i_visible_pitch) /
                p_inpic->format.i_visible_width;
            i_xpos_u = (i_pixel_col * p_inpic->p[U_PLANE].i_visible_pitch) /
                p_inpic->format.i_visible_width;
            i_xpos_v = (i_pixel_col * p_inpic->p[V_PLANE].i_visible_pitch) /
                p_inpic->format.i_visible_width;

            yuv_to_rgb(p_rgb_dst_line_red,
                p_rgb_dst_line_green,
                p_rgb_dst_line_blue,

                p_src_y[i_xpos_y],
                p_src_u[i_xpos_u],
                p_src_v[i_xpos_v]);

            /* +4 because output image should be RGB32 with dword alignment! */
            p_rgb_dst_line_red   += 4;
            p_rgb_dst_line_green += 4;
            p_rgb_dst_line_blue  += 4;
        }
   }

   if(p_sys->b_show_dots)
   {
       for(i_row = 1; i_row < i_row_count; i_row++)
       {
           i_pixel_row = (i_row * p_sys->i_crop_height) / i_row_count
                   + p_sys->i_crop_y_offset;

           i_y_row = (i_pixel_row * p_inpic->p[Y_PLANE].i_visible_lines) /
                   p_inpic->format.i_visible_height;

           p_src_y = p_inpic->p[Y_PLANE].p_pixels +
                   p_inpic->p[Y_PLANE].i_pitch * i_y_row;

           for(i_col = 1; i_col < i_col_count; i_col++)
           {
              i_pixel_col = (i_col * p_sys->i_crop_width) / i_col_count +
                            p_sys->i_crop_x_offset;
              i_xpos_y = (i_pixel_col * p_inpic->p[Y_PLANE].i_visible_pitch) /
                         p_inpic->format.i_visible_width;

              p_src_y[i_xpos_y] = 255;
           }
       }
   }

}


/******************************************************************************
* SaveBitmap: Saves the content of a transferbuffer as Bitmap to disk
*******************************************************************************
* just for debugging
* p_sys -> configuration if Atmo from there the function will get height and
*          width
* p_pixels -> should be the dword aligned BGR(A) image data
* psz_filename -> filename where to store
*/
#if defined(__ATMO_DEBUG__)
void SaveBitmap(filter_sys_t *p_sys, uint8_t *p_pixels, char *psz_filename)
{
    /* for debug out only used*/
    VLC_BITMAPINFO bmp_info;
    BITMAPFILEHEADER  bmp_fileheader;
    FILE *fp_bitmap;

    memset(&bmp_info, 0, sizeof(VLC_BITMAPINFO));
    bmp_info.bmiHeader.biSize = sizeof(VLC_BITMAPINFOHEADER);
    bmp_info.bmiHeader.biSizeImage   = p_sys->i_atmo_height *
                                       p_sys->i_atmo_width * 4;
    bmp_info.bmiHeader.biCompression = BI_RGB;
    bmp_info.bmiHeader.biWidth        = p_sys->i_atmo_width;
    bmp_info.bmiHeader.biHeight       = -p_sys->i_atmo_height;
    bmp_info.bmiHeader.biBitCount     = 32;
    bmp_info.bmiHeader.biPlanes       = 1;

    bmp_fileheader.bfReserved1 = 0;
    bmp_fileheader.bfReserved2 = 0;
    bmp_fileheader.bfSize = sizeof(BITMAPFILEHEADER) +
                            sizeof(VLC_BITMAPINFOHEADER) +
                            bmp_info.bmiHeader.biSizeImage;
    bmp_fileheader.bfType = VLC_TWOCC('B','M');
    bmp_fileheader.bfOffBits = sizeof(BITMAPFILEHEADER) +
                               sizeof(VLC_BITMAPINFOHEADER);

    fp_bitmap = fopen(psz_filename,"wb");
    if( fp_bitmap != NULL)
    {
        fwrite(&bmp_fileheader, sizeof(BITMAPFILEHEADER), 1, fp_bitmap);
        fwrite(&bmp_info.bmiHeader, sizeof(VLC_BITMAPINFOHEADER), 1, fp_bitmap);
        fwrite(p_pixels, bmp_info.bmiHeader.biSizeImage, 1, fp_bitmap);
        fclose(fp_bitmap);
    }
}
#endif


/****************************************************************************
* CreateMiniImage: extracts a 64x48 pixel image from the frame
* (there is a small border arround thats why the loops starts with one
* instead zero) without any interpolation
*****************************************************************************/
static void CreateMiniImage( filter_t *p_filter, picture_t *p_inpic)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    /*
    pointer to RGB Buffer created in external libary as safe array which
    is locked inside AtmoLockTransferBuffer
    */
    uint8_t *p_transfer;
#if defined( __ATMO_DEBUG__ )
    /* for debug out only used*/
    char sz_filename[MAX_PATH];
#endif

    /*
    Lock the before created VarArray (AtmoCreateTransferBuffers)
    inside my wrapper library and give me a pointer to the buffer!
    below linux a global buffer may be used and protected with a mutex?
    */
    p_transfer = AtmoLockTransferBuffer(p_filter);
    if(p_transfer == NULL)
    {
        msg_Err( p_filter, "AtmoLight no transferbuffer available. "\
                           "AtmoLight will be disabled!");
        p_sys->b_enabled = false;
        return;
    }

    /*
    do the call via pointer to function instead of having a
    case structure here
    */
    p_sys->pf_extract_mini_image(p_sys, p_inpic, p_transfer);


#if defined( __ATMO_DEBUG__ )
    /*
    if debugging enabled save every 128th image to disk
    */
    if(p_sys->b_saveframes && p_sys->sz_framepath[0] != 0 )
    {

        if((p_sys->ui_frame_counter & 127) == 0)
        {
            sprintf(sz_filename,"%satmo_dbg_%06u.bmp",p_sys->sz_framepath,
                p_sys->ui_frame_counter);
            msg_Dbg(p_filter, "SaveFrame %s",sz_filename);

            SaveBitmap(p_sys, p_transfer, sz_filename);
        }
    }

    msg_Dbg( p_filter, "AtmoFrame %u Time: %d ms", p_sys->ui_frame_counter,
                mdate() / 1000);
    p_sys->ui_frame_counter++;
#endif

    p_sys->i_frames_processed++;


    /* show the colors on the wall */
    AtmoSendPixelData( p_filter );
}




/*****************************************************************************
* Filter: calls the extract method and forwards the incomming picture 1:1
*****************************************************************************
*
*****************************************************************************/

static picture_t * Filter( filter_t *p_filter, picture_t *p_pic )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    if( !p_pic ) return NULL;

    picture_t *p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }
    picture_CopyPixels( p_outpic, p_pic );

    vlc_mutex_lock( &p_sys->filter_lock );

    if(p_sys->b_enabled && p_sys->pf_extract_mini_image &&
       !p_sys->b_pause_live)
    {
        p_sys->i_crop_x_offset  = p_filter->fmt_in.video.i_x_offset;
        p_sys->i_crop_y_offset  = p_filter->fmt_in.video.i_y_offset;
        p_sys->i_crop_width     = p_filter->fmt_in.video.i_visible_width;
        p_sys->i_crop_height    = p_filter->fmt_in.video.i_visible_height;

        CreateMiniImage(p_filter, p_outpic);
    }

    vlc_mutex_unlock( &p_sys->filter_lock );


    return CopyInfoAndRelease( p_outpic, p_pic );
}


/*****************************************************************************
* FadeToColorThread: Threadmethod which changes slowly the color
* to a target color defined in p_fadethread struct
* use for: Fade to Pause Color,  and Fade to End Color
*****************************************************************************/
static void *FadeToColorThread(void *obj)
{
    fadethread_t *p_fadethread = (fadethread_t *)obj;
    filter_sys_t *p_sys = (filter_sys_t *)p_fadethread->p_filter->p_sys;
    int i_steps_done = 0;
    int i_index;
    int i_pause_red;
    int i_pause_green;
    int i_pause_blue;

    int i_src_red;
    int i_src_green;
    int i_src_blue;

    uint8_t *p_source = NULL;

    int canc = vlc_savecancel ();
    /* initialize AtmoWin for this thread! */
    AtmoInitialize(p_fadethread->p_filter , true);

    uint8_t *p_transfer = AtmoLockTransferBuffer( p_fadethread->p_filter );
    if(p_transfer != NULL) {
        /* safe colors as "32bit" Integers to avoid overflows*/
        i_pause_red   = p_fadethread->ui_red;
        i_pause_blue  = p_fadethread->ui_blue;
        i_pause_green = p_fadethread->ui_green;

        /*
        allocate a temporary buffer for the last send
        image size less then 15kb
        */
        int i_size = 4 * p_sys->i_atmo_width * p_sys->i_atmo_height;
        p_source = (uint8_t *)malloc( i_size );
        if(p_source != NULL)
        {
            /*
            get a copy of the last transfered image as orign for the
            fading steps...
            */
            memcpy(p_source, p_transfer, i_size);
            /* send the same pixel data again... to unlock the buffer! */
            AtmoSendPixelData( p_fadethread->p_filter );

            while( (!vlc_atomic_get (&p_fadethread->abort)) &&
                (i_steps_done < p_fadethread->i_steps))
            {
                p_transfer = AtmoLockTransferBuffer( p_fadethread->p_filter );
                if(!p_transfer) break; /* should not happen if it worked
                                       one time in the code above! */
                i_steps_done++;
                /*
                move all pixels in the mini image (64x48) one step closer to
                the desired color these loop takes the most time of this
                thread improvements wellcome!
                */
                for(i_index = 0;
                    (i_index < i_size) && (!vlc_atomic_get (&p_fadethread->abort));
                    i_index+=4)
                {
                    i_src_blue  = p_source[i_index+0];
                    i_src_green = p_source[i_index+1];
                    i_src_red   = p_source[i_index+2];
                    p_transfer[i_index+0] = (uint8_t) (((
                        (i_pause_blue  - i_src_blue)
                        * i_steps_done)/p_fadethread->i_steps)
                        + i_src_blue);

                    p_transfer[i_index+1] = (uint8_t) (((
                        (i_pause_green - i_src_green)
                        * i_steps_done)/p_fadethread->i_steps)
                        + i_src_green);

                    p_transfer[i_index+2] = (uint8_t) (((
                        (i_pause_red   - i_src_red)
                        * i_steps_done)/p_fadethread->i_steps)
                        + i_src_red);
                }

                /* send image to lightcontroller */
                AtmoSendPixelData( p_fadethread->p_filter );
                /* is there something like and interruptable sleep inside
                the VLC libaries? inside native win32 I would use an Event
                (CreateEvent) and here an WaitForSingleObject?
                */
                msleep(40000);
            }
            free(p_source);
        } else {
            /* in failure of malloc also unlock buffer  */
            AtmoSendPixelData(p_fadethread->p_filter);
        }
    }
    /* call indirect to OleUnitialize() for this thread */
    AtmoFinalize(p_fadethread->p_filter, 0);
    vlc_restorecancel (canc);
    return NULL;
}

/*****************************************************************************
* CheckAndStopFadeThread: if there is a fadethread structure left, or running.
******************************************************************************
* this function will stop the thread ... and waits for its termination
* before removeing the objects from vout_sys_t ...
******************************************************************************/
static void CheckAndStopFadeThread(filter_t *p_filter)
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
    vlc_mutex_lock( &p_sys->filter_lock );
    if(p_sys->p_fadethread != NULL)
    {
        msg_Dbg(p_filter, "kill still running fadeing thread...");

        vlc_atomic_set(&p_sys->p_fadethread->abort, 1);

        vlc_join(p_sys->p_fadethread->thread, NULL);
        free(p_sys->p_fadethread);
        p_sys->p_fadethread = NULL;
    }
    vlc_mutex_unlock( &p_sys->filter_lock );
}

/*****************************************************************************
* StateCallback: Callback for the inputs variable "State" to get notified
* about Pause and Continue Playback events.
*****************************************************************************/
static int StateCallback( vlc_object_t *, char const *,
                         vlc_value_t oldval, vlc_value_t newval,
                         void *p_data )
{
    filter_t *p_filter = (filter_t *)p_data;
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    if(p_sys->b_usepausecolor && p_sys->b_enabled)
    {
        msg_Dbg(p_filter, "state change from: %"PRId64" to %"PRId64, oldval.i_int,
            newval.i_int);

        if((newval.i_int == PAUSE_S) && (oldval.i_int == PLAYING_S))
        {
            /* tell the other thread to stop sending images to light
               controller */
            p_sys->b_pause_live = true;

            // clean up old thread - should not happen....
            CheckAndStopFadeThread( p_filter );

            // perpare spawn fadeing thread
            vlc_mutex_lock( &p_sys->filter_lock );
            /*
            launch only a new thread if there is none active!
            or waiting for cleanup
            */
            if(p_sys->p_fadethread == NULL)
            {
                p_sys->p_fadethread = (fadethread_t *)calloc( 1, sizeof(fadethread_t) );
                p_sys->p_fadethread->p_filter = p_filter;
                p_sys->p_fadethread->ui_red   = p_sys->ui_pausecolor_red;
                p_sys->p_fadethread->ui_green = p_sys->ui_pausecolor_green;
                p_sys->p_fadethread->ui_blue  = p_sys->ui_pausecolor_blue;
                p_sys->p_fadethread->i_steps  = p_sys->i_fadesteps;
                vlc_atomic_set(&p_sys->p_fadethread->abort, 0);

                if( vlc_clone( &p_sys->p_fadethread->thread,
                               FadeToColorThread,
                               p_sys->p_fadethread,
                               VLC_THREAD_PRIORITY_LOW ) )
                {
                    msg_Err( p_filter, "cannot create FadeToColorThread" );
                    free( p_sys->p_fadethread );
                    p_sys->p_fadethread = NULL;
                }
            }
            vlc_mutex_unlock( &p_sys->filter_lock );
        }

        if((newval.i_int == PLAYING_S) && (oldval.i_int == PAUSE_S))
        {
            /* playback continues check thread state */
            CheckAndStopFadeThread( p_filter );
            /* reactivate the Render function... to do its normal work */
            p_sys->b_pause_live = false;
        }
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
* AddPlaylistInputThreadStateCallback: Setup call back on "State" Variable
*****************************************************************************
* Add Callback function to the "state" variable of the input thread..
* first find the PlayList and get the input thread from there to attach
* my callback?
*****************************************************************************/
static void AddStateVariableCallback(filter_t *p_filter)
{
    input_thread_t *p_input = playlist_CurrentInput( pl_Get( p_filter ) );
    if(p_input)
    {
        var_AddCallback( p_input, "state", StateCallback, p_filter );
        vlc_object_release( p_input );
    }
}

/*****************************************************************************
* DelPlaylistInputThreadStateCallback: Remove call back on "State" Variable
*****************************************************************************
* Delete the callback function to the "state" variable of the input thread...
* first find the PlayList and get the input thread from there to attach
* my callback.
*****************************************************************************/
static void DelStateVariableCallback( filter_t *p_filter )
{
    input_thread_t *p_input = playlist_CurrentInput( pl_Get ( p_filter ) );
    if(p_input)
    {
        var_DelCallback( p_input, "state", StateCallback, p_filter );
        vlc_object_release( p_input );
    }
}

/****************************************************************************
* StateCallback: Callback for the inputs variable "State" to get notified
* about Pause and Continue Playback events.
*****************************************************************************/
static int AtmoSettingsCallback( vlc_object_t *, char const *psz_var,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *p_data )
{
    filter_t *p_filter = (filter_t *)p_data;
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    vlc_mutex_lock( &p_sys->filter_lock );

    if( !strcmp( psz_var, CFG_PREFIX "showdots" ))
    {
        p_sys->b_show_dots = newval.b_bool;
    }

    CAtmoConfig *p_atmo_config = p_sys->p_atmo_config;
    if(p_atmo_config)
    {

       msg_Dbg(p_filter, "apply AtmoSettingsCallback %s (int: %"PRId64" -> %"PRId64")",
             psz_var,
             oldval.i_int,
             newval.i_int
       );

        if( !strcmp( psz_var, CFG_PREFIX "filtermode" ))
            p_atmo_config->setLiveViewFilterMode( (AtmoFilterMode)newval.i_int);

        else if( !strcmp( psz_var, CFG_PREFIX "percentnew" ))
                 p_atmo_config->setLiveViewFilter_PercentNew( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "meanlength" ))
                 p_atmo_config->setLiveViewFilter_MeanLength( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "meanthreshold" ))
                 p_atmo_config->setLiveViewFilter_MeanThreshold( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "edgeweightning" ))
                 p_atmo_config->setLiveView_EdgeWeighting( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "brightness" ))
                 p_atmo_config->setLiveView_BrightCorrect( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "darknesslimit" ))
                 p_atmo_config->setLiveView_DarknessLimit( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "huewinsize" ))
                 p_atmo_config->setLiveView_HueWinSize( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "satwinsize" ))
                 p_atmo_config->setLiveView_SatWinSize( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "framedelay" ))
                 p_atmo_config->setLiveView_FrameDelay( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "whiteadj" ))
                 p_atmo_config->setUseSoftwareWhiteAdj( newval.b_bool );

        else if( !strcmp( psz_var, CFG_PREFIX "white-red" ))
                 p_atmo_config->setWhiteAdjustment_Red( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "white-green" ))
                 p_atmo_config->setWhiteAdjustment_Green( newval.i_int );

        else if( !strcmp( psz_var, CFG_PREFIX "white-blue" ))
                 p_atmo_config->setWhiteAdjustment_Blue( newval.i_int );

    }

    vlc_mutex_unlock( &p_sys->filter_lock );

    return VLC_SUCCESS;
}

static void AddAtmoSettingsVariablesCallbacks(filter_t *p_filter)
{
   var_AddCallback( p_filter, CFG_PREFIX "filtermode",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "percentnew",
                    AtmoSettingsCallback, p_filter );


   var_AddCallback( p_filter, CFG_PREFIX "meanlength",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "meanthreshold",
                    AtmoSettingsCallback, p_filter );

   var_AddCallback( p_filter, CFG_PREFIX "edgeweightning",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "brightness",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "darknesslimit",
                    AtmoSettingsCallback, p_filter );

   var_AddCallback( p_filter, CFG_PREFIX "huewinsize",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "satwinsize",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "framedelay",
                    AtmoSettingsCallback, p_filter );


   var_AddCallback( p_filter, CFG_PREFIX "whiteadj",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "white-red",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "white-green",
                    AtmoSettingsCallback, p_filter );
   var_AddCallback( p_filter, CFG_PREFIX "white-blue",
                    AtmoSettingsCallback, p_filter );

   var_AddCallback( p_filter, CFG_PREFIX "showdots",
                    AtmoSettingsCallback, p_filter );

}

static void DelAtmoSettingsVariablesCallbacks( filter_t *p_filter )
{

   var_DelCallback( p_filter, CFG_PREFIX "filtermode",
                    AtmoSettingsCallback, p_filter );

   var_DelCallback( p_filter, CFG_PREFIX "percentnew",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "meanlength",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "meanthreshold",
                    AtmoSettingsCallback, p_filter );

   var_DelCallback( p_filter, CFG_PREFIX "edgeweightning",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "brightness",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "darknesslimit",
                    AtmoSettingsCallback, p_filter );

   var_DelCallback( p_filter, CFG_PREFIX "huewinsize",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "satwinsize",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "framedelay",
                    AtmoSettingsCallback, p_filter );


   var_DelCallback( p_filter, CFG_PREFIX "whiteadj",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "white-red",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "white-green",
                    AtmoSettingsCallback, p_filter );
   var_DelCallback( p_filter, CFG_PREFIX "white-blue",
                    AtmoSettingsCallback, p_filter );

   var_DelCallback( p_filter, CFG_PREFIX "showdots",
                    AtmoSettingsCallback, p_filter );

}


#if defined(__ATMO_DEBUG__)
static void atmo_parse_crop(char *psz_cropconfig,
                            video_format_t fmt_in,
                            video_format_t fmt_render,
                            int &i_visible_width, int &i_visible_height,
                            int &i_x_offset, int &i_y_offset )
{
    int64_t i_aspect_num, i_aspect_den;
    unsigned int i_width, i_height;

    i_visible_width  = fmt_in.i_visible_width;
    i_visible_height = fmt_in.i_visible_height;
    i_x_offset       = fmt_in.i_x_offset;
    i_y_offset       = fmt_in.i_y_offset;

    char *psz_end = NULL, *psz_parser = strchr( psz_cropconfig, ':' );
    if( psz_parser )
    {
        /* We're using the 3:4 syntax */
        i_aspect_num = strtol( psz_cropconfig, &psz_end, 10 );
        if( psz_end == psz_cropconfig || !i_aspect_num ) return;

        i_aspect_den = strtol( ++psz_parser, &psz_end, 10 );
        if( psz_end == psz_parser || !i_aspect_den ) return;

        i_width = fmt_in.i_sar_den * fmt_render.i_visible_height *
            i_aspect_num / i_aspect_den / fmt_in.i_sar_num;

        i_height = fmt_render.i_visible_width*fmt_in.i_sar_num *
            i_aspect_den / i_aspect_num / fmt_in.i_sar_den;

        if( i_width < fmt_render.i_visible_width )
        {
            i_x_offset = fmt_render.i_x_offset +
                (fmt_render.i_visible_width - i_width) / 2;
            i_visible_width = i_width;
        }
        else
        {
            i_y_offset = fmt_render.i_y_offset +
                (fmt_render.i_visible_height - i_height) / 2;
            i_visible_height = i_height;
        }
    }
    else
    {
        psz_parser = strchr( psz_cropconfig, 'x' );
        if( psz_parser )
        {
            /* Maybe we're using the <width>x<height>+<left>+<top> syntax */
            unsigned int i_crop_width, i_crop_height, i_crop_top, i_crop_left;

            i_crop_width = strtol( psz_cropconfig, &psz_end, 10 );
            if( psz_end != psz_parser ) return;

            psz_parser = strchr( ++psz_end, '+' );
            i_crop_height = strtol( psz_end, &psz_end, 10 );
            if( psz_end != psz_parser ) return;

            psz_parser = strchr( ++psz_end, '+' );
            i_crop_left = strtol( psz_end, &psz_end, 10 );
            if( psz_end != psz_parser ) return;

            psz_end++;
            i_crop_top = strtol( psz_end, &psz_end, 10 );
            if( *psz_end != '\0' ) return;

            i_width = i_crop_width;
            i_visible_width = i_width;

            i_height = i_crop_height;
            i_visible_height = i_height;

            i_x_offset = i_crop_left;
            i_y_offset = i_crop_top;
        }
        else
        {
            /* Maybe we're using the <left>+<top>+<right>+<bottom> syntax */
            unsigned int i_crop_top, i_crop_left, i_crop_bottom, i_crop_right;

            psz_parser = strchr( psz_cropconfig, '+' );
            i_crop_left = strtol( psz_cropconfig, &psz_end, 10 );
            if( psz_end != psz_parser ) return;

            psz_parser = strchr( ++psz_end, '+' );
            i_crop_top = strtol( psz_end, &psz_end, 10 );
            if( psz_end != psz_parser ) return;

            psz_parser = strchr( ++psz_end, '+' );
            i_crop_right = strtol( psz_end, &psz_end, 10 );
            if( psz_end != psz_parser ) return;

            psz_end++;
            i_crop_bottom = strtol( psz_end, &psz_end, 10 );
            if( *psz_end != '\0' ) return;

            i_width = fmt_render.i_visible_width -
                        i_crop_left -
                        i_crop_right;
            i_visible_width = i_width;

            i_height = fmt_render.i_visible_height -
                        i_crop_top -
                        i_crop_bottom;
            i_visible_height = i_height;

            i_x_offset = i_crop_left;
            i_y_offset = i_crop_top;
        }
    }
}
#endif
