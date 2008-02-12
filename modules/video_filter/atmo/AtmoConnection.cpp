/*
 * AtmoConnection.cpp: generic/abstract class defining all methods for the
 * communication with the hardware
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#include "AtmoConnection.h"

CAtmoConnection::CAtmoConnection(CAtmoConfig *cfg)
{
	this->m_pAtmoConfig = cfg;	
    if(cfg->getNumChannelAssignments()>0) {
        tChannelAssignment *ca = cfg->getChannelAssignment(0);
        for(int i=0;i<ATMO_NUM_CHANNELS;i++) {
            m_ChannelAssignment[i] = ca->mappings[i];
        }
    } else {
        for(int i=0;i<ATMO_NUM_CHANNELS;i++) {
            m_ChannelAssignment[i] = i;
        }
    }
}

void CAtmoConnection::SetChannelAssignment(tChannelAssignment *ca) {
     for(int i=0;i<ATMO_NUM_CHANNELS;i++) {
         m_ChannelAssignment[i] = ca->mappings[i];
     }
}

CAtmoConnection::~CAtmoConnection(void)
{
  if(isOpen())
     CloseConnection();
}
