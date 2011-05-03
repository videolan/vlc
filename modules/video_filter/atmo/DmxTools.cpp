/*
 * DmxTools.cpp: functions to convert , or ; separated numbers into an integer array
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "DmxTools.h"

int IsValidDmxStartString(char *startChannels)
{
  if(!startChannels) return -1;

  char channel[16];
  int tmp;
  int ch = 0;
  int i = 0;

  while(*startChannels) {
      if(*startChannels == ',' || *startChannels == ';') {
         if(i > 0) {
            channel[i] = 0;
            // atoi ?
            tmp = atoi(channel);
            if((tmp >= 0) && (tmp<=253))
                ch++;
            else
                return -2; // invalid channel number!
            i = 0;
         }
      }
      else
      {
        if((*startChannels >= '0') && (*startChannels <= '9')) {
            if(i < 3)
                channel[i++] = *startChannels;
            else
                return -3; // invalid index length!
        } else {
            if(*startChannels != ' ') {
                return -4; // invalid character found!
            }
        }
      }
      startChannels++;
  }

  // process the rest (or last channel)
  if(!*startChannels && (i>0)) {
      channel[i] = 0;
      tmp = atoi(channel);
      if((tmp >= 0) && (tmp<=253)) {
         ch++;
      } else
         return -2;
  }

  return ch;
}

int *ConvertDmxStartChannelsToInt(int numChannels, char *startChannels)
{
  if(!numChannels || !startChannels) return NULL;
  int *channels = new int[numChannels + 1];
  // tmp buffer to store channel number!
  char channel[16];
  int next_dmx_ch = 0;
  int tmp;
  int ch = 0;
  int i = 0;

  while(*startChannels) {
      if(*startChannels == ',' || *startChannels == ';') {
         if(i > 0) {
            channel[i] = 0;
            // atoi ?
            tmp = atoi(channel);
            if((tmp >= 0) && (tmp<=253)) {
                next_dmx_ch = tmp + 3;
                channels[ch++] = tmp;
                if(ch == numChannels)
                   break;
            } else
                break;
            i = 0;
         }
      }
      if((*startChannels >= '0') && (*startChannels <= '9')) {
         if(i < 3)
            channel[i++] = *startChannels;
         else
            break;
      }
      startChannels++;
  }

  if(!*startChannels && (i>0)) {
      channel[i] = 0;
      tmp = atoi(channel);
      if((tmp >= 0) && (tmp<=253)) {
         next_dmx_ch = tmp + 3;
         channels[ch++] = tmp;
      }
  }
  //
  // fillup the array with the logical next dmx channels - for simple devices that should work!
  //
  while(ch < numChannels) {
        if(next_dmx_ch>253) next_dmx_ch=0; // wrap arround :) better than indexing memory out of range
        channels[ch++] = next_dmx_ch;
        next_dmx_ch += 3;
  }
  channels[ch++] = -1; // last Entry :)

  return channels;
}

char *ConvertDmxStartChannelsToString(int numChannels, int *startChannels)
{
  // maxBufSize worst case having numChannels 256 each 3 digits Adress and one colon 256*4 bytes + zero byte
  char tmp[1025];
  // fuck up! (should not happen)
  if(numChannels > 256) return NULL;


  char *psz_tmp = tmp;
  for(int i = 0; i < numChannels; i++) {
      if(startChannels[i] == -1) break;
      if(i > 0) {
         *psz_tmp = ',';
         psz_tmp++;
         *psz_tmp = 0;
      }
      int n = sprintf(psz_tmp, "%d", startChannels[i] );
      if(n > 0)
         psz_tmp += n;
  }

  return strdup(tmp);
}

