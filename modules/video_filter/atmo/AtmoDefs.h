/*
 * AtmoDefs.h: a lot of globals defines for the color computation - most of this file
 * is an one  to one copy of "defs.h" from Atmo VDR Plugin
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifndef _AtmoDefs_h_
#define _AtmoDefs_h_

#if defined(__LIBVLC__)
#   include "config.h"

#   define __STDC_CONSTANT_MACROS 1
#   include <inttypes.h>

#   include <vlc_common.h>

/* some things need to be changed if this code is used inside VideoLan Filter Module */
#   define _ATMO_VLC_PLUGIN_
#   define ATMO_BOOL bool
#   define ATMO_TRUE true
#   define ATMO_FALSE false

#else

    typedef int ATMO_BOOL;
#   define ATMO_TRUE   1
#   define ATMO_FALSE  0
#   define MakeWord(ch1,ch2)  ((((int)(ch1)&255)<<8) | \
                           ((int)(ch2)&255))

#   define MakeDword(ch1,ch2,ch3,ch4) ((((DWORD)(ch1)&255) << 24) | \
                                   (((DWORD)(ch2)&255) << 16) | \
                                   (((DWORD)(ch3)&255) << 8) | \
                                   (((DWORD)(ch4)&255)))


#endif


#if !defined(WIN32)

#define INVALID_HANDLE_VALUE -1
typedef int HANDLE;
typedef unsigned long DWORD;

#define BI_RGB 0L

#if !defined(_BITMAPFILEHEADER_)
#define _BITMAPFILEHEADER_
typedef struct
#ifdef HAVE_ATTRIBUTE_PACKED
    __attribute__((__packed__))
#endif
{
        uint16_t   bfType;
        uint32_t   bfSize;
        uint16_t   bfReserved1;
        uint16_t   bfReserved2;
        uint32_t   bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER, *PBITMAPFILEHEADER;
#endif

#endif







// maximal Anzahl Kanäle...
#define ATMO_NUM_CHANNELS   5

// capture width/height
#define CAP_WIDTH    64
#define CAP_HEIGHT   48

// imagesize
#define IMAGE_SIZE   (CAP_WIDTH * CAP_HEIGHT)


enum AtmoConnectionType
{
      actSerialPort = 0,
      actDummy = 1,
      actDMX = 2
};
static const char *AtmoDeviceTypes[] = {
      "Atmo",
      "Dummy",
      "DMX"
  };
#define ATMO_DEVICE_COUNT 3

#if defined(_ATMO_VLC_PLUGIN_)
enum EffectMode {
      emUndefined = -1,
      emDisabled = 0,
      emStaticColor = 1,
      emLivePicture = 2
   };
#else
enum EffectMode {
      emUndefined = -1,
      emDisabled = 0,
      emStaticColor = 1,
      emLivePicture = 2,
      emColorChange = 3,
      emLrColorChange = 4
   };
#endif




enum AtmoFilterMode {
     afmNoFilter,
     afmCombined,
     afmPercent
};

typedef struct {
    ATMO_BOOL system;
    char name[64];
    int mappings[ATMO_NUM_CHANNELS];
} tChannelAssignment;


// --- tRGBColor --------------------------------------------------------------
typedef struct
{
  unsigned char r, g, b;
} tRGBColor;

// --- tColorPacket -----------------------------------------------------------
typedef struct
{
  tRGBColor channel[ATMO_NUM_CHANNELS];
} tColorPacket;

// --- tRGBColorLongInt -------------------------------------------------------
typedef struct
{
  long int r, g, b;
} tRGBColorLongInt;

// --- tColorPacketLongInt ----------------------------------------------------
typedef struct
{
  tRGBColorLongInt channel[ATMO_NUM_CHANNELS];
} tColorPacketLongInt;

// --- tWeightPacket ----------------------------------------------------------
typedef struct
{
  int channel[ATMO_NUM_CHANNELS];
} tWeightPacket;

// --- tHSVColor --------------------------------------------------------------
typedef struct
{
  unsigned char h, s, v;
} tHSVColor;

#endif
