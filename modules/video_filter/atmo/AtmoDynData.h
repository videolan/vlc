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

#include <stdio.h>

#include "AtmoDefs.h"

#include "AtmoThread.h"
#include "AtmoConfig.h"
#include "AtmoConnection.h"
#include "AtmoPacketQueue.h"
#include "AtmoInput.h"

#if !defined(_ATMO_VLC_PLUGIN_)
#    include "AtmoDisplays.h"
#else
#   include <vlc_common.h>
#   include <vlc_threads.h>
#endif

class CAtmoInput;

/*
  the idea behind this class is to avoid a mix of persistent value and
  volatile values in CAtmoConfig class because some parameters and variables
  exists only for the current process and won't be stored to the registry

  (Simple thought its a container... )

  you ask? why I didn't used a struct for it? ..mmh I like classes?

  Always stop the current effect Thread before changing AtmoConnection or
  AtmoConfig!
*/
class CAtmoDynData
{
private:
    /*
      thread creating the current output (depends on active effect)
    */
    CThread *m_pCurrentEffectThread;

    /*
      in Modus Live View the packetQueue is the connection
      between the output processing and the pixelsource
    */
    CAtmoPacketQueue *m_pLivePacketQueue;

    /*
      thread for getting and preparing the pixeldata in color
      packets for each zone
    */
    CAtmoInput *m_pLiveInput;
    LivePictureSource m_LivePictureSource;

    /*
    connection to the current configure hardware device
    */
    CAtmoConnection *m_pAtmoConnection;

    /*
     all global persistent parameters
    */
    CAtmoConfig *m_pAtmoConfig;

#if !defined(_ATMO_VLC_PLUGIN_)
    CAtmoDisplays *m_pAtmoDisplays;
    HINSTANCE m_hInst;
    CRITICAL_SECTION m_RemoteCallCriticalSection;
    char m_WorkDir[MAX_PATH];
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


    CAtmoPacketQueue *getLivePacketQueue() { return m_pLivePacketQueue; }
    void setLivePacketQueue(CAtmoPacketQueue *pQueue) { m_pLivePacketQueue = pQueue; }

    CAtmoInput *getLiveInput() { return m_pLiveInput; }
    void setLiveInput(CAtmoInput *value) {  m_pLiveInput = value; }

    LivePictureSource getLivePictureSource() { return m_LivePictureSource; }
    void setLivePictureSource(LivePictureSource lps) { m_LivePictureSource = lps; }

    CAtmoConnection *getAtmoConnection() { return m_pAtmoConnection; }
    void setAtmoConnection(CAtmoConnection *value) { m_pAtmoConnection = value; }

    CAtmoConfig *getAtmoConfig() { return m_pAtmoConfig; }

    void ReloadZoneDefinitionBitmaps();
    void CalculateDefaultZones();

#if !defined(_ATMO_VLC_PLUGIN_)
    CAtmoDisplays *getAtmoDisplays() { return m_pAtmoDisplays; }
    HINSTANCE getHinstance() { return m_hInst; }
    void setWorkDir(const char *dir);
    char *getWorkDir();
#else
    vlc_object_t *getAtmoFilter() { return p_atmo_filter; }
#endif

    void LockCriticalSection();
    void UnLockCriticalSection();
};

#endif
