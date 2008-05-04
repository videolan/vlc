/*
 * AtmoDynData.cpp: class for holding all variable data - which may be
 * passed between function calls, into threads instead of the use
 * of global variables
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#include "AtmoDynData.h"

#if defined(_ATMO_VLC_PLUGIN_)
CAtmoDynData::CAtmoDynData(vlc_object_t *p_atmo_filter, CAtmoConfig *pAtmoConfig) {
    this->p_atmo_filter     = p_atmo_filter;
    this->m_pAtmoConfig     = pAtmoConfig;
    this->m_pAtmoConnection = NULL;
    this->m_pCurrentEffectThread = NULL;

    vlc_mutex_init( &m_lock );

}
#else
CAtmoDynData::CAtmoDynData(HINSTANCE hInst, CAtmoConfig *pAtmoConfig, CAtmoDisplays *pAtmoDisplays) {
    this->m_pAtmoConfig     = pAtmoConfig;
    this->m_pAtmoDisplays   = pAtmoDisplays;
    this->m_pAtmoConnection = NULL;
    this->m_pCurrentEffectThread = NULL;
    this->m_hInst = hInst;
    InitializeCriticalSection(&m_RemoteCallCriticalSection);
}
#endif

CAtmoDynData::~CAtmoDynData(void)
{
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_destroy( &m_lock );
#else
    DeleteCriticalSection(&m_RemoteCallCriticalSection);
#endif
}

void CAtmoDynData::LockCriticalSection() {
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_lock( &m_lock );
#else
    EnterCriticalSection(&m_RemoteCallCriticalSection);
#endif
}

void CAtmoDynData::UnLockCriticalSection() {
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_unlock( &m_lock );
#else
    LeaveCriticalSection(&m_RemoteCallCriticalSection);
#endif
}
