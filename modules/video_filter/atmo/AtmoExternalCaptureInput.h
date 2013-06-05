#ifndef _AtmoExternalCaptureInput_h_
#define _AtmoExternalCaptureInput_h_

#include "AtmoDefs.h"

#if defined(_WIN32)
#   include <windows.h>
# else
#   if defined(_ATMO_VLC_PLUGIN_)
       // need bitmap info header
#      include <vlc_codecs.h>
#   endif
#endif

#if defined(_ATMO_VLC_PLUGIN_)
#  include <vlc_common.h>
#  include <vlc_threads.h>
#endif

#include "AtmoInput.h"
#include "AtmoThread.h"
#include "AtmoConfig.h"
#include "AtmoDynData.h"
#include "AtmoCalculations.h"


class CAtmoExternalCaptureInput :  public CAtmoInput
{
protected:
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_cond_t   m_WakeupCond;
    vlc_mutex_t  m_WakeupLock;
    vlc_object_t *m_pLog;
#else
    HANDLE m_hWakeupEvent;
    CRITICAL_SECTION m_BufferLock;
#endif

    VLC_BITMAPINFOHEADER m_CurrentFrameHeader;
    void *m_pCurrentFramePixels;

    virtual DWORD Execute(void);
    void CalcColors();

public:
    /*
       this method is called from the com server AtmoLiveViewControlImpl!
       or inside videolan from the filter method to start a new processing
    */
    void DeliverNewSourceDataPaket(VLC_BITMAPINFOHEADER *bmpInfoHeader,void *pixelData);

public:
    CAtmoExternalCaptureInput(CAtmoDynData *pAtmoDynData);
    virtual ~CAtmoExternalCaptureInput(void);

    /*
       Opens the input-device. Parameters (e.g. the device-name)
       Returns true if the input-device was opened successfully.
       input-device can be the GDI surface of screen (windows only)
       or the videolan filter
    */
    virtual ATMO_BOOL Open(void);

    /*
     Closes the input-device.
     Returns true if the input-device was closed successfully.
    */
    virtual ATMO_BOOL Close(void);

};

#endif
