/*
 * AtmoChannelAssignment.cpp: Class for storing a hardware channel to zone mapping
 * List
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "AtmoChannelAssignment.h"


CAtmoChannelAssignment::CAtmoChannelAssignment(void)
{
  m_psz_name = strdup("");
  m_mappings = NULL;
  m_num_channels = 0;
  system = ATMO_FALSE;
}

CAtmoChannelAssignment::CAtmoChannelAssignment(CAtmoChannelAssignment &source)
{
  m_num_channels = 0; m_psz_name = NULL;
  m_mappings = source.getMapArrayClone(m_num_channels);
  setName( source.getName() );
  system = source.system;
}

CAtmoChannelAssignment::~CAtmoChannelAssignment(void)
{
  free(m_psz_name);
}

void CAtmoChannelAssignment::setName(const char *pszNewName)
{
  free(m_psz_name);
  m_psz_name = pszNewName ? strdup(pszNewName) : strdup("");
}

void CAtmoChannelAssignment::setSize(int numChannels)
{
  if(numChannels != m_num_channels)
  {
     delete []m_mappings;
     m_mappings = NULL;
     m_num_channels = numChannels;
     if(m_num_channels > 0)
     {
       m_mappings = new int[m_num_channels];
       memset(m_mappings, 0, sizeof(int) * m_num_channels);
     }
  }
}

int *CAtmoChannelAssignment::getMapArrayClone(int &count)
{
  count = m_num_channels;
  if(count == 0) return NULL;
  int *temp = new int[m_num_channels];
  memcpy(temp, m_mappings, sizeof(int) * m_num_channels);
  return(temp);
}

int CAtmoChannelAssignment::getZoneIndex(int channel)
{
   if(m_mappings && (channel>=0) && (channel<m_num_channels))
     return m_mappings[channel] ;
   else
     return -1;
}

void CAtmoChannelAssignment::setZoneIndex(int channel, int zone)
{
 if(m_mappings && (channel>=0) && (channel<m_num_channels))
    m_mappings[channel] = zone;
}


