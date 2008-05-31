/*
 * AtmoDynData.h: class for holding all variable data - which may be passed
 * between function calls, into threads instead of the use of global variables
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoDynData_h_
#define _AtmoDynData_h_

#include "AtmoDefs.h"

#include "AtmoThread.h"
#include "AtmoConfig.h"
#include "AtmoConnection.h"

#if !defined(_ATMO_VLC_PLUGIN_)
#    include "AtmoDisplays.h"
#else
#    include <vlc_common.h>
#    include <vlc_threads.h>
#endif

/*
  the idea behind this class is to avoid a mix of persistent value and
  volatile values in CAtmoConfig class because some parameters and variables
  exists only for the current process and won't be stored to the registry

  (Simple thought its a container... )

  you ask? why I didn't used a struct for it? ..mmh I like classes?

  Problem: MultiThreading! todo semaphore, mutex!

  Allways stop the current effect Thread before changing AtmoConnection or
  AtmoConfig!
*/
class CAtmoDynData
{
private:
    CThread *m_pCurrentEffectThread;
    CAtmoConnection *m_pAtmoConnection;
    CAtmoConfig *m_pAtmoConfig;

#if !defined(_ATMO_VLC_PLUGIN_)
    CAtmoDisplays *m_pAtmoDisplays;
    HINSTANCE m_hInst;
    CRITICAL_SECTION m_RemoteCallCriticalSection;
#else
    vlc_object_t *p_atmo_filter;
    vlc_mutex_t  m_lock;
#endif


public:
#if !defined(_ATMO_VLC_PLUGIN_)
     CAtmoDynData(HINSTANCE hInst,
                  CAtmoConfig *pAtmoConfig,
                  CAtmoDisplays *pAtmoDisplays);
#else
     CAtmoDynData(vlc_object_t *p_atmo_filter,
                  CAtmoConfig *pAtmoConfig);
#endif
    ~CAtmoDynData(void);

    CThread *getEffectThread()           { return m_pCurrentEffectThread; }
    void setEffectThread(CThread *value) { m_pCurrentEffectThread = value; }

    CAtmoConnection *getAtmoConnection() { return m_pAtmoConnection; }
    void setAtmoConnection(CAtmoConnection *value) { m_pAtmoConnection = value; }

    CAtmoConfig *getAtmoConfig() { return m_pAtmoConfig; }

#if !defined(_ATMO_VLC_PLUGIN_)
    CAtmoDisplays *getAtmoDisplays() { return m_pAtmoDisplays; }
    HINSTANCE getHinstance() { return m_hInst; }
#else
    vlc_object_t *getAtmoFilter() { return p_atmo_filter; }
#endif

    void LockCriticalSection();
    void UnLockCriticalSection();
};

#endif
