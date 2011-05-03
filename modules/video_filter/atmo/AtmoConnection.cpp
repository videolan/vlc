/*
 * AtmoConnection.cpp: generic/abstract class defining all methods for the
 * communication with the hardware
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include "AtmoConnection.h"


CAtmoConnection::CAtmoConnection(CAtmoConfig *cfg)
{
	 this->m_pAtmoConfig = cfg;	
     m_ChannelAssignment = NULL;
     m_NumAssignedChannels = 0;

#if defined(_ATMO_VLC_PLUGIN_)
     vlc_mutex_init( &m_AccessConnection );
#else
     InitializeCriticalSection( &m_AccessConnection );
#endif
}

CAtmoConnection::~CAtmoConnection(void)
{
  if(isOpen())
     CloseConnection();

#if defined(_ATMO_VLC_PLUGIN_)
     vlc_mutex_destroy( &m_AccessConnection );
#else
     DeleteCriticalSection( &m_AccessConnection );
#endif
}

void CAtmoConnection::SetChannelAssignment(CAtmoChannelAssignment *ca)
{
  if(ca)
  {
      Lock();
      delete m_ChannelAssignment;
      m_ChannelAssignment = ca->getMapArrayClone(m_NumAssignedChannels);
      Unlock();
  }
}

#if !defined(_ATMO_VLC_PLUGIN_)
ATMO_BOOL CAtmoConnection::ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg)
{
    MessageBox(parent, "This device doesn't have a special config dialog", "Info", 0);
    return ATMO_FALSE;
}
#endif


void CAtmoConnection::Lock()
{
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_lock( &m_AccessConnection );
#else
    EnterCriticalSection( &m_AccessConnection );
#endif
}
void CAtmoConnection::Unlock()
{
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_unlock( &m_AccessConnection );
#else
    LeaveCriticalSection( &m_AccessConnection );
#endif
}
